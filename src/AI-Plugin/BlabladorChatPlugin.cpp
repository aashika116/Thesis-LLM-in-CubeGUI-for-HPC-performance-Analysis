/****************************************************************************

What this does (briefly):
    - AI plugin for Cubegui
    - Uses Blablador API to analyze call tree for load imbalance
    - Fetches call tree data directly from GUI
    - Sequentially processes call paths with their corresponding system tree values so we can ensure 
    focused analysis in the LLM and avoid running out of tokens/context window
    - Provides AI response: imbalance % for each call path, reasons for imbalance and suggestions to prove/disprove 
    each reason.

What it's supposed to do (briefly):
    - The above plus,
    - Mark imbalanced call paths in the Call tree with the Item marker plugin
    - Set label to the marked item with imbalance % and reason for imbalance

****************************************************************************/

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

#include <functional> 

#include "TreeItem.h"
#include "TreeItemMarker.h"

// Global pointer to store the "Load imbalance" marker
const cubepluginapi::TreeItemMarker* imbalanceMarker = nullptr;

// Constants
namespace {
    constexpr const char* API_URL = "https://api.helmholtz-blablador.fz-juelich.de/v1/chat/completions";
    constexpr const char* MODEL_NAME = "01 - GPT-OSS-120b - an open model released by OpenAI in August 2025";

constexpr const char* SYSTEM_PROMPT = R"(
You are an HPC load imbalance analysis engine embedded in CubeGUI.

You analyze EXACTLY ONE call path.
Every request is completely independent.
You must reason ONLY from the supplied statistics.
Do not invent information that is not present.

The input contains:
- metric name
- number of ranks
- average runtime
- minimum runtime
- maximum runtime
- standard deviation
- imbalance percentage
- worst rank
- top slow ranks
- runtime distribution summary

Definitions:

imbalance_percent = ((max_runtime - avg_runtime) / max_runtime) * 100

Interpretation:
0-10%      Balanced
10-30%     Moderate imbalance
>30%       Severe imbalance

Reasoning rules:
- Use only the supplied distribution.
- Do not assume communication imbalance unless the distribution supports it.
- Do not assume computational imbalance unless the distribution supports it.
- Distinguish between:
  - single-rank outliers
  - small groups of slow ranks
  - widespread imbalance

Output format:

CASE 1: Balanced (0-10%)
call_path: "<call_path>"
severity: <percent>%
classification: Balanced

CASE 2: Moderate imbalance (10-30%)
call_path: "<call_path>"
severity: <percent>%
classification: Moderate Imbalance
slowest_ranks: <rank list>
reason: <one sentence explaining what the runtime distribution indicates>
investigate: <one sentence describing exactly what profiling data should be compared>
fix: <one sentence describing a realistic balancing action> 

CASE 3: Severe imbalance (>30%)
call_path: "<call_path>"
severity: <percent>%
classification: Severe Imbalance
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

- Analyze ONLY the provided call path
- Treat every request as stateless
- Output ONLY the required format
- No markdown
- No bullet points
- No extra commentary
- No JSON
)";
}

enum RequestType {
    AutoAnalysisRequest
};

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

// Automatic Analysis
void BlabladorChatPlugin::checkLoadImbalance() {
    outputField->setPlainText("Analyzing call tree...");
    QCoreApplication::processEvents();

    fetchCallTreeFromGUI(); // Rebuilds internal dataset
    if (callTreeTextList.isEmpty()) {
        outputField->appendPlainText("No call tree data.");
        return;
    }

    // Start auto analysis loop, initializes sequential processing state
    currentCallIndex = 0;
    autoAnalyzing = true;

    // starts pipeline
    sendNextCallPath();
}

// Send next call path to API
void BlabladorChatPlugin::sendNextCallPath() {
    if (!autoAnalyzing || currentCallIndex >= callTreeTextList.size()) return; // stops if cancelled or finished

    QString apiKey = QProcessEnvironment::systemEnvironment().value("BLABLADOR_API_KEY");
    if (apiKey.isEmpty()) {
        outputField->appendPlainText("API key missing.");
        return;
    }

    // Iterates sequentially over call paths to ensure that AI processes one path at a time
    QString currentCallPath = callTreeTextList[currentCallIndex];

    QNetworkRequest request{ QUrl{ API_URL } };
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());

    QJsonArray messages;
    messages.append(QJsonObject{{"role","system"},{"content", SYSTEM_PROMPT}});
    QString wrapped =
    "=== CALL PATH START ===\n" +
    currentCallPath +
    "\n=== CALL PATH END ===";

    messages.append(QJsonObject{{"role","user"},{"content", wrapped}});

    QJsonObject payload{{"model", MODEL_NAME}, {"messages", messages}};
    request.setAttribute(QNetworkRequest::User, AutoAnalysisRequest);
    network->post(request, QJsonDocument(payload).toJson());
}

// Handle API Response
void BlabladorChatPlugin::handleApiResponse(QNetworkReply* reply) {
    if (!reply) return;
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
    outputField->appendPlainText(
        "\n====================================\n"
        "CALL PATH " + QString::number(currentCallIndex + 1) +
        "\n------------------------------------\n" +
        responseText +
        "\n"
    );
    QCoreApplication::processEvents();

    if (!autoAnalyzing)
    return;

    currentCallIndex++;

    if (currentCallIndex < callTreeTextList.size()) {
        sendNextCallPath();
    }
    else {
        outputField->appendPlainText(
            "\nFinished analyzing all call paths."
        );

        autoAnalyzing = false;
        currentCallIndex = 0;
    }
}

// Fetch Call Tree from CubeGUI
void BlabladorChatPlugin::fetchCallTreeFromGUI() {
    callTreeTextList.clear();
    if (!service) return;

    auto callRoots = service->getTopLevelItems(cubepluginapi::CALL);
    auto systemRoots = service->getTopLevelItems(cubepluginapi::SYSTEM);

    // Collect ALL SYSTEM LEAFS (MPI ranks)
    QList<cubepluginapi::TreeItem*> systemLeafs;

    std::function<void(cubepluginapi::TreeItem*)> collectSystemLeafs;

    collectSystemLeafs = [&](cubepluginapi::TreeItem* sysNode) {
        if (!sysNode) return;

        if (sysNode->getChildren().isEmpty()) {
            systemLeafs.append(sysNode);
            return;
        }

        for (auto* child : sysNode->getChildren()) {
            collectSystemLeafs(child);
        }
    };

    for (auto* root : systemRoots) {
        collectSystemLeafs(root);
    }

    // Traverse CALL TREE
    std::function<void(cubepluginapi::TreeItem*)> collectCallPaths;

    collectCallPaths = [&](cubepluginapi::TreeItem* callNode) {
        if (!callNode) return;

        QString text;

        QVector<double> values;
        QVector<int> ranks;

        callNode->select(false);

        int rank = 0;

        for (auto* sysLeaf : systemLeafs) {
            // Clear old selections
            callNode->deselect();
            sysLeaf->deselect();

            // Select EXACT combination:
            // current call path + current rank
            callNode->select(false);
            sysLeaf->select(true);

            // Let CubeGUI recompute values
            QCoreApplication::processEvents();

            // read value FROM SYSTEM NODE
            double val = sysLeaf->getValue();
            if (std::isfinite(val)) {
                values.append(val);
                ranks.append(rank);

                QString rankInfo =
                    QString("rank_%1 (%2)")
                    .arg(rank)
                    .arg(sysLeaf->getName());
            }

            rank++;
        }

        if (values.size() < 2)
            return;

        // LOCAL imbalance analysis
        double sum = std::accumulate(values.begin(), values.end(), 0.0);

        double avg = sum / values.size();

        auto maxIt = std::max_element(values.begin(), values.end());

        double maxVal = *maxIt;

        int maxIndex = std::distance(values.begin(), maxIt);

        int worstRank = ranks[maxIndex];

        double imbalance =
            ((maxVal - avg) / maxVal) * 100.0;

        double minVal =
            *std::min_element(values.begin(), values.end());

        double variance = 0.0;

        for (double v : values) {
            double diff = v - avg;
            variance += diff * diff;
        }

        variance /= values.size();
        double stddev = std::sqrt(variance);
        double threshold = avg + 2.0 * stddev;

        QStringList slowRanks;

        for (int i = 0; i < values.size(); ++i) {
            if (values[i] > threshold) {
                slowRanks << QString::number(ranks[i]);
            }
        }

        int nearAverage = 0;
        int moderateSlow = 0;
        int extremeSlow = 0;

        for (double v : values) {
            if (v <= avg + stddev) {
                nearAverage++;
            }
            else if (v <= avg + 2.0 * stddev) {
                moderateSlow++;
            }
            else {
                extremeSlow++;
            }
        }

        // Build SMALL prompt
        text += "metric: \"" + callNode->getName() + "\"\n";

        text += "num_ranks: "
            + QString::number(values.size()) + "\n";

        text += "avg_runtime: "
            + QString::number(avg, 'f', 6) + "\n";

        text += "min_runtime: "
            + QString::number(minVal, 'f', 6) + "\n";

        text += "max_runtime: "
            + QString::number(maxVal, 'f', 6) + "\n";

        text += "stddev_runtime: "
            + QString::number(stddev, 'f', 6) + "\n";

        text += "imbalance_percent: "
            + QString::number(imbalance, 'f', 2) + "\n";

        text += "worst_rank: "
            + QString::number(worstRank) + "\n";

        text += "top_slow_ranks: ";

        if (slowRanks.isEmpty()) {
            text += "none\n";
        }
        else {
            text += slowRanks.join(", ") + "\n";
        }

        text += "distribution_summary:\n";

        text += "near_average_ranks: "
            + QString::number(nearAverage) + "\n";

        text += "moderately_slow_ranks: "
            + QString::number(moderateSlow) + "\n";

        text += "extremely_slow_ranks: "
            + QString::number(extremeSlow) + "\n";

        outputField->appendPlainText(
            "\nDEBUG " + callNode->getName() +
            "\nvalues collected: " +
            QString::number(values.size()) +
            "\nimbalance: " +
            QString::number(imbalance, 'f', 2) + "%"
        );

        callTreeTextList.append(text);

        for (auto* child : callNode->getChildren()) {
            collectCallPaths(child);
        }
    };

    for (auto* root : callRoots) {
        collectCallPaths(root);
    }
}