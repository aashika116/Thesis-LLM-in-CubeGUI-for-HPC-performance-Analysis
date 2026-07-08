#ifndef PARSETOJSONPLUGIN_H
#define PARSETOJSONPLUGIN_H

#include <QObject>
#include <QtPlugin>
#include <QPlainTextEdit>

#include "PluginServices.h"
#include "CubePlugin.h"
#include "TabInterface.h"

namespace cube {
    class SystemTreeNode;
}

class ParseToJSONPlugin : public QObject, public cubepluginapi::CubePlugin, public cubepluginapi::TabInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "ParseToJSONPlugin")
    Q_INTERFACES(cubepluginapi::CubePlugin)

public:
    ParseToJSONPlugin();
    ~ParseToJSONPlugin();

    bool cubeOpened(cubepluginapi::PluginServices* service) override;
    void cubeClosed() override;

    void version(int& major, int& minor, int& bugfix) const override;
    QString name() const override;
    QString getHelpText() const override;

    QWidget* widget() override;
    QString label() const override;

private:
    QPlainTextEdit* jsonViewer;

    QString readSystemTreeJson(const QString& cubexFilePath);
    void traverseSystemTree(const cube::SystemTreeNode* node, QJsonObject& jsonObject);

    void addBasicProperties(const cube::SystemTreeNode* node, QJsonObject& jsonObject);
    void addChildNodes(const cube::SystemTreeNode* node, QJsonObject& jsonObject);
    void addLocationGroups(const cube::SystemTreeNode* node, QJsonObject& jsonObject);
};

#endif
