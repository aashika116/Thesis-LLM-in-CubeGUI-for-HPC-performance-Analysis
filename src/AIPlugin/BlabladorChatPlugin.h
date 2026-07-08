#ifndef BLABLADOR_CHAT_PLUGIN_H
#define BLABLADOR_CHAT_PLUGIN_H

#include <QObject>
#include <QToolButton>
#include <QNetworkAccessManager>
#include <QPlainTextEdit>
#include <QList>
#include <QJsonArray>

#include "PluginServices.h"
#include "CubePlugin.h"
#include "TabInterface.h"

#include <QLineEdit>
#include <QPushButton>

#include <QMap>
#include <QStringList>

class QWidget;
class QVBoxLayout;
class QNetworkReply;

enum RequestType {
    RegionAnalysisRequest
};

class BlabladorChatPlugin final :
    public QObject,
    public cubepluginapi::CubePlugin,
    public cubepluginapi::TabInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "BlabladorChatPlugin")
    Q_INTERFACES(cubepluginapi::CubePlugin)

private:
    cubepluginapi::PluginServices* service { nullptr };

    // CubeGUI Marker helpers
    void markCallPath(const QString &callPathId, const QString &label);

    // Read call tree directly from GUI
    void fetchCallTreeFromGUI();

    // Stores each call path as a text block
    // QList<QString> callTreeTextList;

    // UI elements
    QLineEdit* userInputField { nullptr };
    QPushButton* sendButton { nullptr };
    QToolButton* checkImbalanceButton { nullptr };
    QPlainTextEdit* outputField { nullptr };
    QWidget* mainWidget { nullptr };

    // Network manager
    QNetworkAccessManager* network { nullptr };

    // Chat memory
    QJsonArray conversationHistory; // manual chat (temporary)
    QJsonArray autoConversationHistory;
    bool autoAnalyzing = false;

    // int currentCallIndex = 0;

    QList<cubepluginapi::TreeItem*> systemLeafs;

    QMap<QString, cubepluginapi::TreeItem*> callPathMap;

    QStringList selectedPaths;

    int currentSelectedIndex = 0;

public:
    BlabladorChatPlugin();
    ~BlabladorChatPlugin() override;

    // CubePlugin interface
    bool cubeOpened(cubepluginapi::PluginServices* service) override;
    void cubeClosed() override;
    void version(int& major, int& minor, int& bugfix) const override;
    QString name() const override;
    QString getHelpText() const override;

    // TabInterface
    QWidget* widget() override;
    QString label() const override;

private slots:
    void handleApiResponse(QNetworkReply* reply);
    void checkLoadImbalance();
    void sendUserMessage();
  
    void sendRegionAnalysisRequest();

    QString buildFullPath(cubepluginapi::TreeItem* node);
    QString buildRankVectorPrompt(cubepluginapi::TreeItem* callNode);

private:
    void setupUi();
    void setupConnections();
    void setupNetwork();
    void sendToApi();
};

#endif