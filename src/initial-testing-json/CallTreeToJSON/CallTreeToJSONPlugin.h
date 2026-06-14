#ifndef CALLTREETOJSONPLUGIN_H
#define CALLTREETOJSONPLUGIN_H

#include <QObject>
#include <QPlainTextEdit>

#include "PluginServices.h"
#include "CubePlugin.h"
#include "TabInterface.h"

namespace cube {
    class Cnode;
    class Cube;
    class Metric;
    class Region;
    class Location;
}

class CallTreeToJSONPlugin
    : public QObject
    , public cubepluginapi::CubePlugin
    , public cubepluginapi::TabInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "CallTreeToJSONPlugin")
    Q_INTERFACES(cubepluginapi::CubePlugin)

public:
    CallTreeToJSONPlugin();
    ~CallTreeToJSONPlugin();

    bool cubeOpened(cubepluginapi::PluginServices* service) override;
    void cubeClosed() override;
    void version(int& major, int& minor, int& bugfix) const override;
    QString name() const override;
    QString getHelpText() const override;

    QWidget* widget() override;
    QString label() const override;

private:
    QPlainTextEdit* jsonViewer;

    QString readCallTreeJson(const QString& cubexFilePath);

    void traverseCallTree(
        cube::Cube& cube,
        cube::Metric* metric,
        const cube::Cnode* cnode,
        const std::vector<cube::Location*>& locations,
        QJsonObject& jsonObject
    );
};

#endif