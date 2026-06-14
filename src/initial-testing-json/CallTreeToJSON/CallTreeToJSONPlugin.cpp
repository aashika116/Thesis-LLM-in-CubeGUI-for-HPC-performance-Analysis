#include "CallTreeToJSONPlugin.h"
#include "PluginServices.h"

#include <QMessageBox>
#include <QUrl>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QPlainTextEdit>
#include <QFontDatabase>

#include <cmath>
#include <Cube.h>

// Plugin metadata
void CallTreeToJSONPlugin::version(int& major, int& minor, int& bugfix) const {
    major = 1;
    minor = 2;
    bugfix = 0;
}

QString CallTreeToJSONPlugin::name() const {
    return "CallTreeToJSONPlugin";
}

QString CallTreeToJSONPlugin::getHelpText() const {
    return "Exports the call tree with per-CPU inclusive time values as JSON (seconds).";
}

// Constructor / Destructor
CallTreeToJSONPlugin::CallTreeToJSONPlugin() {
    jsonViewer = new QPlainTextEdit();
    jsonViewer->setReadOnly(true);
    jsonViewer->setWordWrapMode(QTextOption::NoWrap);
    jsonViewer->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
}

CallTreeToJSONPlugin::~CallTreeToJSONPlugin() {
    delete jsonViewer;
}

// CubeGUI plugin interface
bool CallTreeToJSONPlugin::cubeOpened(cubepluginapi::PluginServices* service) {
    service->addTab(cubepluginapi::SYSTEM, this);

    QString cubexFilePath = service->getCubeFileName();
    if (cubexFilePath.isEmpty()) {
        QMessageBox::warning(nullptr, "Error", "No .cubex file provided.");
        return false;
    }

    QString jsonContent = readCallTreeJson(cubexFilePath);
    if (!jsonContent.isEmpty()) {
        jsonViewer->setPlainText(jsonContent);

        // Auto-export JSON
        QFile outFile("/home/aashika/Documents/thesis/official/master-thesis-llm-in-cubegui-for-performance-analysis-in-hpc/src/Extras/CallTreeJSON.json");
        if (outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            outFile.write(jsonContent.toUtf8());
            outFile.close();
        }
    }

    return true;
}

void CallTreeToJSONPlugin::cubeClosed() {
    jsonViewer->clear();
}

// JSON conversion
QString CallTreeToJSONPlugin::readCallTreeJson(const QString& cubexFilePath) {
    QUrl fileUrl(cubexFilePath);
    QString localFilePath = fileUrl.toLocalFile();

    if (!QFile::exists(localFilePath)) {
        QMessageBox::warning(nullptr, "Error", "The specified .cubex file does not exist.");
        return QString();
    }

    cube::Cube cube;
    try {
        cube.openCubeReport(localFilePath.toStdString());
    } catch (const std::exception& e) {
        QMessageBox::warning(nullptr, "Error", e.what());
        return QString();
    }

    // Select time metric
    cube::Metric* timeMetric = nullptr;
    for (auto* m : cube.get_metv()) {
        if (m->get_uniq_name() == "time") {
            timeMetric = m;
            break;
        }
    }

    if (!timeMetric) {
        QMessageBox::warning(nullptr, "Error", "Time metric not found.");
        return QString();
    }

    // Collect all locations (MPI ranks / threads)
    const auto& locations = cube.get_locationv();

    // Traverse call tree
    QJsonArray callTreeArray;
    for (const auto* root : cube.get_root_cnodev()) {
        QJsonObject rootJson;
        traverseCallTree(cube, timeMetric, root, locations, rootJson);
        callTreeArray.append(rootJson);
    }

    QJsonObject fullJson;
    fullJson["CallTree"] = callTreeArray;

    return QString(QJsonDocument(fullJson).toJson(QJsonDocument::Indented));
}

// Traversal
void CallTreeToJSONPlugin::traverseCallTree(
    cube::Cube& cube,
    cube::Metric* metric,
    const cube::Cnode* cnode,
    const std::vector<cube::Location*>& locations,
    QJsonObject& jsonObject
) {
    const cube::Region* region = cnode->get_callee();

    jsonObject["id"] = static_cast<int>(cnode->get_id());
    jsonObject["name"] = QString::fromStdString(region->get_name());
    jsonObject["mod"] = QString::fromStdString(region->get_mod());

    if (const auto* parent = cnode->get_parent()) {
        jsonObject["parent"] = QString::fromStdString(parent->get_callee()->get_name());
    } else {
        jsonObject["parent"] = QJsonValue::Null;
    }

    // Per-location inclusive metric values
    QJsonArray perCpuArray;
    for (const auto* loc : locations) {
        double value = cube.get_sev(metric, cnode, loc);
        value = std::round(value * 1e6) / 1e6; // round to microseconds
        perCpuArray.append(value);
    }
    jsonObject["per_cpu"] = perCpuArray;

    // Children
    QJsonArray childrenArray;
    for (unsigned int i = 0; i < cnode->num_children(); ++i) {
        QJsonObject childJson;
        traverseCallTree(cube, metric, cnode->get_child(i), locations, childJson);
        childrenArray.append(childJson);
    }
    jsonObject["children"] = childrenArray;
}

// Tab interface
QWidget* CallTreeToJSONPlugin::widget() {
    return jsonViewer;
}

QString CallTreeToJSONPlugin::label() const {
    return "Call Tree JSON";
}