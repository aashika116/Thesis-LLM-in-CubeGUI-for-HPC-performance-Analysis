/***************************************************************************************************************

- Created in April 2025 as an assignment for the thesis role.
- This file is a plugin that converts the system tree data from CubeGUI to JSON format.
- The JSONs (ParseToJSONPlugin.cpp and CallTreeToJSONPlugin.cpp) were then used as test input in various LLMs 
to check whether the existing LLMs withoug requiring futher training can detect load imbalance and provide 
reasons and validation steps with structured data.

***************************************************************************************************************/

#include "ParseToJSONPlugin.h"
#include "PluginServices.h"
#include <QMessageBox>
#include <QUrl>
#include <QFile>

#include <Cube.h>
#include <QJsonObject>
#include <QJsonDocument>
#include <QPlainTextEdit>

void ParseToJSONPlugin::version(int& major, int& minor, int& bugfix) const
{
    major = 1;
    minor = 0;
    bugfix = 0;
}

// Returns the name of the plugin
QString ParseToJSONPlugin::name() const
{
    return "ParseToJSONPlugin";
}
 
// Returns the help text for the plugin
QString ParseToJSONPlugin::getHelpText() const
{
    return "This plugin visualizes the system tree of a .cubex file in JSON format.";
}

ParseToJSONPlugin::ParseToJSONPlugin()
{
    jsonViewer = new QPlainTextEdit();
    jsonViewer->setReadOnly(true);
    jsonViewer->setWordWrapMode(QTextOption::NoWrap);
    jsonViewer->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
}

ParseToJSONPlugin::~ParseToJSONPlugin()
{
    delete jsonViewer;
}

bool ParseToJSONPlugin::cubeOpened(cubepluginapi::PluginServices* service)
{
    service->addTab(cubepluginapi::SYSTEM, this);

    QString cubexFilePath = service->getCubeFileName();
    if (cubexFilePath.isEmpty()) {
        QMessageBox::warning(nullptr, "Error", "No .cubex file provided.");
        return false;
    }

    QString jsonContent = readSystemTreeJson(cubexFilePath);
    if (!jsonContent.isEmpty()) {
        jsonViewer->setPlainText(jsonContent);
    }

    return true;
}

void ParseToJSONPlugin::cubeClosed()
{
    jsonViewer->clear();
}

QString ParseToJSONPlugin::readSystemTreeJson(const QString& cubexFilePath)
{
    QUrl fileUrl(cubexFilePath);
    QString localFilePath = fileUrl.toLocalFile();
    
    if (!QFile::exists(localFilePath)) {
        QMessageBox::warning(nullptr, "Error", "The specified .cubex file does not exist:\n" + localFilePath);
        return QString();
    }

    cube::Cube cube;
    
    try {
        cube.openCubeReport(localFilePath.toStdString());
    } catch (const std::exception& e) {
        QMessageBox::warning(nullptr, "Error", "Failed to open cubex file:\n" + QString::fromStdString(e.what()));
        return QString();
    }

    QJsonArray systemTreeArray;
    const auto& roots = cube.get_root_stnv();

    for (const auto& root : roots) {
        QJsonObject rootJson;
        traverseSystemTree(root, rootJson);
        systemTreeArray.append(rootJson);
    }

    QJsonObject fullJson;
    fullJson["System"] = systemTreeArray;
    return QString(QJsonDocument(fullJson).toJson(QJsonDocument::Indented));
}

void ParseToJSONPlugin::traverseSystemTree(const cube::SystemTreeNode* node, QJsonObject& jsonObject)
{
    addBasicProperties(node, jsonObject);
    addChildNodes(node, jsonObject);
    addLocationGroups(node, jsonObject);
}

void ParseToJSONPlugin::addBasicProperties(const cube::SystemTreeNode* node, QJsonObject& jsonObject)
{
    jsonObject["name"] = QString::fromStdString(node->get_name());
    jsonObject["id"] = static_cast<int>(node->get_id());
    jsonObject["description"] = QString::fromStdString(node->get_desc());
    jsonObject["class"] = QString::fromStdString(node->get_class());

    // Add parent node
    if (auto* parent = node->get_parent()) {
        jsonObject["parent"] = QString::fromStdString(parent->get_name());
    } else {
        jsonObject["parent"] = QJsonValue::Null;
    }
}

void ParseToJSONPlugin::addChildNodes(const cube::SystemTreeNode* node, QJsonObject& jsonObject)
{
    QJsonArray childrenArray;

    // Iterate through all child nodes and traverse them recursively
    for (unsigned int i = 0; i < node->num_children(); ++i) {
        QJsonObject childJson;
        traverseSystemTree(node->get_child(i), childJson);
        childrenArray.append(childJson);
    }
    jsonObject["children"] = childrenArray;
}

void ParseToJSONPlugin::addLocationGroups(const cube::SystemTreeNode* node, QJsonObject& jsonObject)
{
    QJsonArray groupsArray;

    // Iterate through all location groups associated with the node
    for (const auto& group : const_cast<cube::SystemTreeNode*>(node)->get_groups()) {
        QJsonObject groupJson;
        QJsonArray locationsArray;
        
        groupJson["name"] = QString::fromStdString(group->get_name());
        groupJson["id"] = static_cast<int>(group->get_id());

        // Iterate through all locations in the group
        for (const auto& location : group->get_children()) {
            QJsonObject locationJson;
            locationJson["name"] = QString("Location %1").arg(location->get_id());
            locationJson["id"] = static_cast<int>(location->get_id());
            locationsArray.append(locationJson);
        }
        groupJson["locations"] = locationsArray;

        groupsArray.append(groupJson);
    }
    jsonObject["location_groups"] = groupsArray;
}

QWidget* ParseToJSONPlugin::widget()
{
    return jsonViewer;
}

QString ParseToJSONPlugin::label() const
{
    return "System Tree JSON";
}
