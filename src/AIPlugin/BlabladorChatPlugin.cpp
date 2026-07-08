/***************************************************************************************************************

What this does (briefly):
    - AI plugin for CubeGUI.
    - Uses Blablador API to analyze load imbalance
    - GUI analyzes call tree and selects regions contributing >5% to total runtime
    - For each selected region, fetches the runtime and sends it to the LLM for analysis
    - Provides a detailed response for each region, including:
        - call path name
        - severity (imbalance percentage)
        - slowest rank list
        - reasons for imbalance
        - plausible steps to investigate and fix imbalance
    - Marks the imbalanced call paths and labels them as "Load Imbalanced"

Limitations/Future work:
    - Mark the call path and label with the imbalance percentage and reason for imbalance
    - For call trees with more than one root, analyze each root better (now it considers an average of 
    all roots contributing >5% to total runtime)
    - Maximum number of ranks it works for now: 8192. Takes approximately 45 minutes.

***************************************************************************************************************/

#include "BlabladorChatPlugin.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QProcessEnvironment>
#include <QCoreApplication>

#include <QElapsedTimer>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <QDebug>

#include <functional> 

#include "TreeItem.h"
#include "TreeItemMarker.h"

#include<CubeProxy.h>

// Global pointer to store the "Load imbalance" marker
const cubepluginapi::TreeItemMarker* imbalanceMarker = nullptr;

// Constants
namespace {
    constexpr const char* API_URL = "https://api.helmholtz-blablador.fz-juelich.de/v1/chat/completions";
    constexpr const char* MODEL_NAME = "01 - GPT-OSS-120b - an open model released by OpenAI in August 2025";

constexpr const char* REGION_ANALYSIS_PROMPT = R"(

You analyze EXACTLY ONE call path.

Input:
- call_path
- collapsed_runtime
- expanded_runtime
- runtimes

IMPORTANT:
runtimes[i] corresponds to rank i.

You must compute:
- average runtime
- minimum runtime
- maximum runtime
- standard deviation
- imbalance percentage

Definition:
imbalance_percent = ((max_runtime - average_runtime) / max_runtime) * 100

Output format:

call_path: "<call_path>"
severity: <percent>%
slowest_ranks: <rank list>
reason: <one sentence explaining what the runtime distribution indicates>
investigate: <one sentence describing exactly what profiling data should be compared>
fix: <one sentence describing a realistic balancing action>

--------------------------------------------------
INVESTIGATE/FIX RULES (VERY IMPORTANT)

The investigate sentence MUST explain HOW to verify the imbalance using profiling data.

GOOD investigate examples:
- compare per-rank iteration counts and workload sizes for the listed ranks
- inspect message sizes and synchronization delays on the slowest ranks
- compare computational work assigned to the slowest ranks

BAD investigate examples:
- profile the application
- inspect the program
- check performance

The fix sentence MUST describe a SPECIFIC balancing strategy.

GOOD fix examples:
- redistribute mesh partitions more evenly across MPI ranks
- rebalance particle ownership among the slowest ranks
- reduce synchronization caused by overloaded ranks

BAD fix examples:
- optimize the code
- improve performance
- rebalance workload

--------------------------------------------------
PROCESSING RULES
- Output ONLY the required format
- No markdown
- No bullet points
- No extra commentary
- No JSON
)";
}

// Constructor
BlabladorChatPlugin::BlabladorChatPlugin() {
    setupUi();
    setupNetwork();
    setupConnections();
}

// Destructor
BlabladorChatPlugin::~BlabladorChatPlugin() = default;

// CubePlugin Interface
bool BlabladorChatPlugin::cubeOpened(cubepluginapi::PluginServices* service) {
    this->service = service;
    service->addTab(cubepluginapi::SYSTEM, this);

    imbalanceMarker = service->getTreeItemMarker("Load imbalance");

    fetchCallTreeFromGUI();
    return true;
}

void BlabladorChatPlugin::cubeClosed() {
}

void BlabladorChatPlugin::version(int& major, int& minor, int& bugfix) const {
    major = 1;
    minor = 3;
    bugfix = 0;
}

QString BlabladorChatPlugin::name() const {
    return "BlabladorAI";
}

QString BlabladorChatPlugin::getHelpText() const {
    return "Analyze call tree for load imbalance.";
}

QWidget* BlabladorChatPlugin::widget() {
    return mainWidget;
}

QString BlabladorChatPlugin::label() const {
    return "Blablador AI";
}

// UI setup
void BlabladorChatPlugin::setupUi() {
    mainWidget = new QWidget();
    auto* mainLayout = new QVBoxLayout(mainWidget);

    checkImbalanceButton = new QToolButton();
    checkImbalanceButton->setText("Analyze Load Imbalance");

    outputField = new QPlainTextEdit();
    outputField->setReadOnly(true);

    mainLayout->addWidget(checkImbalanceButton);
    mainLayout->addWidget(outputField);
}

// Network setup
void BlabladorChatPlugin::setupNetwork() {
    network = new QNetworkAccessManager(this);
}

// Signal-Slot Connections
void BlabladorChatPlugin::setupConnections() {
    connect(network, &QNetworkAccessManager::finished, this, &BlabladorChatPlugin::handleApiResponse);

    connect(checkImbalanceButton, &QToolButton::clicked, this, &BlabladorChatPlugin::checkLoadImbalance);
}

// Entry point for automatic load imbalance analysis.
   /* 1. Rebuild call tree.
   2. CubeGUI selects regions contributing >5%.
   3. Analyze selected regions one-by-one. */
void BlabladorChatPlugin::checkLoadImbalance() {
    outputField->setPlainText("Analyzing call tree...");
    QCoreApplication::processEvents();

    fetchCallTreeFromGUI(); // Rebuilds internal dataset

    currentSelectedIndex = 0;
    autoAnalyzing = true;

    sendRegionAnalysisRequest();
}

// Build the prompt for detailed analysis of a specific call path
QString BlabladorChatPlugin::buildRankVectorPrompt(
    cubepluginapi::TreeItem* callNode) {
    QString text;

    QString fullPath = buildFullPath(callNode);

    text +=
        "call_path: \"" +
        fullPath +
        "\"\n";

    text +=
        "collapsed_runtime: " +
        QString::number(
            callNode->getTotalValue(),
            'f',
            6)
        + "\n";

    text +=
        "expanded_runtime: " +
        QString::number(
            callNode->getOwnValue(),
            'f',
            6)
        + "\n\n";

    text += "runtimes:\n[";

    bool first = true;

    // Iterate over all MPI ranks and collect the runtime for this call path on each rank.
    for (auto* sysLeaf : systemLeafs) {
        callNode->deselect();
        sysLeaf->deselect();

        callNode->select(false);
        sysLeaf->select(true);

        QCoreApplication::processEvents();

        double value = sysLeaf->getValue();

        if (!std::isfinite(value))
            value = 0.0;

        if (!first)
            text += ",";

        text += QString::number(
            value,
            'f',
            6);

        first = false;
    }

    text += "]\n";

    return text;
}

void BlabladorChatPlugin::sendRegionAnalysisRequest() {
    if (currentSelectedIndex >= selectedPaths.size()) {
        outputField->appendPlainText(
            "\nFinished analyzing all regions.");

        autoAnalyzing = false;

        return;
    }

    QString path = selectedPaths[currentSelectedIndex];
    // DEBUG: Show which region is being analyzed
    qDebug()
    << "[ANALYZING REGION]"
    << currentSelectedIndex
    << path;

    cubepluginapi::TreeItem* node =
    callPathMap.value(path, nullptr);

    if (!node) {
        currentSelectedIndex++;
        sendRegionAnalysisRequest();
        return;
    }

    QString prompt = buildRankVectorPrompt(node);

    // DEBUG: Show the prompt sent to the LLM
    qDebug() << "\n========== PROMPT ==========";
    qDebug().noquote() << prompt;
    qDebug() << "============================\n";

    QString apiKey = QProcessEnvironment::systemEnvironment().value("BLABLADOR_API_KEY");

    QNetworkRequest request{QUrl{API_URL}};

    request.setHeader(
        QNetworkRequest::ContentTypeHeader,
        "application/json");

    request.setRawHeader(
        "Authorization",
        ("Bearer " + apiKey).toUtf8());

    QJsonArray messages;

    messages.append(
        QJsonObject{
            {"role","system"},
            {"content", REGION_ANALYSIS_PROMPT}
        });

    messages.append(
        QJsonObject{
            {"role","user"},
            {"content", prompt}
        });

    QJsonObject payload{{"model", MODEL_NAME}, {"messages", messages}};
    request.setAttribute(QNetworkRequest::User, RegionAnalysisRequest);
    network->post(request, QJsonDocument(payload).toJson());
}

// 
void BlabladorChatPlugin::handleApiResponse(QNetworkReply* reply) {
    if (!reply)
        return;

    if (reply->error() != QNetworkReply::NoError) {
        outputField->appendPlainText(
            "Network error: " + reply->errorString()
        );

        QByteArray errData = reply->readAll();

        outputField->appendPlainText(
            "Server response: " + QString(errData)
        );

        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    // DEBUG: Show the raw JSON response from Blablador
    qDebug() << "\n========== RAW RESPONSE ==========";
    qDebug().noquote() << data;
    qDebug() << "==================================\n";

    reply->deleteLater();

    QJsonDocument json = QJsonDocument::fromJson(data);
    if (!json.isObject()) {
        outputField->appendPlainText("Invalid API response.");
        return;
    }

    auto choices = json["choices"].toArray();
    if (choices.isEmpty()) {
        outputField->appendPlainText("No response.");
        return;
    }

    QString responseText = choices[0].toObject()["message"].toObject()["content"].toString();

    // DEBUG: Show only the LLM text
    qDebug() << "\n========== LLM OUTPUT ==========";
    qDebug().noquote() << responseText;
    qDebug() << "================================\n";

    // PHASE 2: detailed load imbalance analysis
    outputField->appendPlainText(
        "\n====================================\n"
        "REGION " + QString::number(currentSelectedIndex + 1) +
        "\n------------------------------------\n" +
        responseText +
        "\n"
    );
    QCoreApplication::processEvents();

    currentSelectedIndex++;

    if (currentSelectedIndex < selectedPaths.size()) {
        sendRegionAnalysisRequest();
    }
    else {
        outputField->appendPlainText(
            "\nFinished analyzing all selected regions.");

        autoAnalyzing = false;
        currentSelectedIndex = 0;
    }
}

QString BlabladorChatPlugin::buildFullPath(cubepluginapi::TreeItem* node) {
    QStringList parts;

    while (node) {
        parts.prepend(node->getName());
        node = node->getParent();
    }

    return parts.join("/");
}

//
void BlabladorChatPlugin::fetchCallTreeFromGUI() {
    selectedPaths.clear();
    callPathMap.clear();
    systemLeafs.clear();

    if (!service)
        return;

    auto callRoots = service->getTopLevelItems(cubepluginapi::CALL);
    qDebug() << "Top-level roots:";

    for (auto *root : callRoots) {
        qDebug().noquote()
            << root->getName()
            << " total =" << root->getTotalValue()
            << " own =" << root->getOwnValue();
    }

    auto systemRoots = service->getTopLevelItems(cubepluginapi::SYSTEM);

    // Collect all system leaf nodes once
    //
    std::function<void(cubepluginapi::TreeItem*)> collectSystemLeafs;

    collectSystemLeafs = [&](cubepluginapi::TreeItem* node) {
        if (!node)
            return;

        if (node->getChildren().isEmpty()) {
            systemLeafs.append(node);
            return;
        }

        for (auto* child : node->getChildren())
            collectSystemLeafs(child);
    };

    for (auto* root : systemRoots)
        collectSystemLeafs(root);

    qDebug() << "System leaves:" << systemLeafs.size();
    // Traverse call tree, walks through every call path in the Cube call tree and
    // collects data needed for the first LLM pass.
    std::function<void(cubepluginapi::TreeItem*)> traverse;

    double totalRuntime = 0.0;

    if (!callRoots.isEmpty()) {

        for (auto* root : callRoots) {
            double runtime = root->getTotalValue();

            if (!std::isfinite(runtime) || runtime <= 0.0)
                runtime = root->getOwnValue();

            if (std::isfinite(runtime) && runtime > 0.0)
                totalRuntime += runtime;
        }
        
        totalRuntime /= callRoots.size();
    }

    traverse = [&](cubepluginapi::TreeItem* node) {
        if (!node)
            return;

        QString fullPath = buildFullPath(node);

        // collapsed runtime
        double collapsedRuntime = node->getTotalValue();
        if (!std::isfinite(collapsedRuntime))
            collapsedRuntime = node->getOwnValue();

        // expanded runtime
        double expandedRuntime = node->getOwnValue();

        callPathMap[fullPath] = node;

        // DEBUG
        qDebug().noquote()
            << fullPath
            << " total =" << collapsedRuntime
            << " own =" << expandedRuntime;

        if (std::isfinite(totalRuntime) && totalRuntime > 0.0) {

            double contributionPercent = (collapsedRuntime / totalRuntime) * 100.0;

            qDebug().noquote()
                << "[CALLPATH]"
                << fullPath
                << "collapsed =" << collapsedRuntime
                << "expanded =" << expandedRuntime
                << "percent =" << contributionPercent;

            if (contributionPercent > 5.0) {
                // DEBUG: Show which regions CubeGUI selected (>5%)
                qDebug()
                    << "[SELECTED]"
                    << fullPath
                    << "collapsed="
                    << collapsedRuntime
                    << "percent="
                    << contributionPercent;

                selectedPaths.append(fullPath);
            }
        }

        for (auto* child : node->getChildren())
            traverse(child);
    };

    for (auto* root : callRoots)
    traverse(root);

    // DEBUG
    qDebug() << "\n===== SELECTED PATHS =====";
    qDebug() << "Number selected:" << selectedPaths.size();

    for (int i = 0; i < selectedPaths.size(); ++i) {
        qDebug().noquote()
            << i << ":"
            << selectedPaths[i];
    }

    qDebug() << "==========================\n";

    // Add "Load Imbalance" marker too selected call paths.
    if (imbalanceMarker && service) {
        for (const QString& path : selectedPaths) {
            cubepluginapi::TreeItem* node = callPathMap.value(path, nullptr);
            if (!node)
                continue;

            service->addMarker(node, imbalanceMarker);
        }
    }
}