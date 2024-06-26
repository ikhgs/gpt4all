#ifndef CHATLLM_H
#define CHATLLM_H

#include "database.h" // IWYU pragma: keep
#include "modellist.h"

#include "../gpt4all-backend/llmodel.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QThread>
#include <QVector>
#include <QtGlobal>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

using namespace Qt::Literals::StringLiterals;

class QDataStream;

enum LLModelType {
    GPTJ_,
    LLAMA_,
    API_,
};

class ChatLLM;

struct LLModelInfo {
    std::unique_ptr<LLModel> model;
    QFileInfo fileInfo;
    std::optional<QString> fallbackReason;

    // NOTE: This does not store the model type or name on purpose as this is left for ChatLLM which
    // must be able to serialize the information even if it is in the unloaded state

    void resetModel(ChatLLM *cllm, LLModel *model = nullptr);
};

class TokenTimer : public QObject {
    Q_OBJECT
public:
    explicit TokenTimer(QObject *parent)
        : QObject(parent)
        , m_elapsed(0) {}

    static int rollingAverage(int oldAvg, int newNumber, int n)
    {
        // i.e. to calculate the new average after then nth number,
        // you multiply the old average by n−1, add the new number, and divide the total by n.
        return qRound(((float(oldAvg) * (n - 1)) + newNumber) / float(n));
    }

    void start() { m_tokens = 0; m_elapsed = 0; m_time.invalidate(); }
    void stop() { handleTimeout(); }
    void inc() {
        if (!m_time.isValid())
            m_time.start();
        ++m_tokens;
        if (m_time.elapsed() > 999)
            handleTimeout();
    }

Q_SIGNALS:
    void report(const QString &speed);

private Q_SLOTS:
    void handleTimeout()
    {
        m_elapsed += m_time.restart();
        emit report(u"%1 tokens/sec"_s.arg(m_tokens / float(m_elapsed / 1000.0f), 0, 'g', 2));
    }

private:
    QElapsedTimer m_time;
    qint64 m_elapsed;
    quint32 m_tokens;
};

class Chat;
class ChatLLM : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isRecalc READ isRecalc NOTIFY recalcChanged)
    Q_PROPERTY(std::optional<QString> deviceBackend READ deviceBackend NOTIFY loadedModelInfoChanged)
    Q_PROPERTY(std::optional<QString> device READ device NOTIFY loadedModelInfoChanged)
    Q_PROPERTY(std::optional<QString> fallbackReason READ fallbackReason NOTIFY loadedModelInfoChanged)
public:
    ChatLLM(Chat *parent, bool isServer = false);
    virtual ~ChatLLM();

    void destroy();
    static void destroyStore();
    bool isModelLoaded() const;
    void regenerateResponse();
    void resetResponse();
    void resetContext();

    void stopGenerating() { m_stopGenerating = true; }

    bool shouldBeLoaded() const { return m_shouldBeLoaded; }
    void setShouldBeLoaded(bool b);
    void requestTrySwitchContext();
    void setForceUnloadModel(bool b) { m_forceUnloadModel = b; }
    void setMarkedForDeletion(bool b) { m_markedForDeletion = b; }

    QString response() const;

    ModelInfo modelInfo() const;
    void setModelInfo(const ModelInfo &info);

    bool isRecalc() const { return m_isRecalc; }

    void acquireModel();
    void resetModel();

    std::optional<QString> deviceBackend() const {
        if (!isModelLoaded()) return std::nullopt;
        std::string name = LLModel::GPUDevice::backendIdToName(m_llModelInfo.model->backendName());
        return QString::fromStdString(name);
    }

    std::optional<QString> device() const {
        if (!isModelLoaded()) return std::nullopt;
        const char *name = m_llModelInfo.model->gpuDeviceName();
        return name ? name : "CPU";
    }

    // not loaded -> null, no fallback -> empty string
    std::optional<QString> fallbackReason() const {
        if (!isModelLoaded()) return std::nullopt;
        return m_llModelInfo.fallbackReason;
    }

    QString generatedName() const { return QString::fromStdString(m_nameResponse); }

    bool serialize(QDataStream &stream, int version, bool serializeKV);
    bool deserialize(QDataStream &stream, int version, bool deserializeKV, bool discardKV);
    void setStateFromText(const QVector<QPair<QString, QString>> &stateFromText) { m_stateFromText = stateFromText; }

public Q_SLOTS:
    bool prompt(const QList<QString> &collectionList, const QString &prompt);
    bool loadDefaultModel();
    void trySwitchContextOfLoadedModel(const ModelInfo &modelInfo);
    bool loadModel(const ModelInfo &modelInfo);
    void modelChangeRequested(const ModelInfo &modelInfo);
    void unloadModel();
    void reloadModel();
    void generateName();
    void handleChatIdChanged(const QString &id);
    void handleShouldBeLoadedChanged();
    void handleThreadStarted();
    void handleForceMetalChanged(bool forceMetal);
    void handleDeviceChanged();
    void processSystemPrompt();
    void processRestoreStateFromText();

Q_SIGNALS:
    void recalcChanged();
    void loadedModelInfoChanged();
    void modelLoadingPercentageChanged(float);
    void modelLoadingError(const QString &error);
    void modelLoadingWarning(const QString &warning);
    void responseChanged(const QString &response);
    void promptProcessing();
    void responseStopped(qint64 promptResponseMs);
    void generatedNameChanged(const QString &name);
    void stateChanged();
    void threadStarted();
    void shouldBeLoadedChanged();
    void trySwitchContextRequested(const ModelInfo &modelInfo);
    void trySwitchContextOfLoadedModelCompleted(int value);
    void requestRetrieveFromDB(const QList<QString> &collections, const QString &text, int retrievalSize, QList<ResultInfo> *results);
    void reportSpeed(const QString &speed);
    void reportDevice(const QString &device);
    void reportFallbackReason(const QString &fallbackReason);
    void databaseResultsChanged(const QList<ResultInfo>&);
    void modelInfoChanged(const ModelInfo &modelInfo);

protected:
    bool promptInternal(const QList<QString> &collectionList, const QString &prompt, const QString &promptTemplate,
        int32_t n_predict, int32_t top_k, float top_p, float min_p, float temp, int32_t n_batch, float repeat_penalty,
        int32_t repeat_penalty_tokens);
    bool handlePrompt(int32_t token);
    bool handleResponse(int32_t token, const std::string &response);
    bool handleRecalculate(bool isRecalc);
    bool handleNamePrompt(int32_t token);
    bool handleNameResponse(int32_t token, const std::string &response);
    bool handleNameRecalculate(bool isRecalc);
    bool handleSystemPrompt(int32_t token);
    bool handleSystemResponse(int32_t token, const std::string &response);
    bool handleSystemRecalculate(bool isRecalc);
    bool handleRestoreStateFromTextPrompt(int32_t token);
    bool handleRestoreStateFromTextResponse(int32_t token, const std::string &response);
    bool handleRestoreStateFromTextRecalculate(bool isRecalc);
    void saveState();
    void restoreState();

protected:
    LLModel::PromptContext m_ctx;
    quint32 m_promptTokens;
    quint32 m_promptResponseTokens;

private:
    std::string m_response;
    std::string m_nameResponse;
    LLModelInfo m_llModelInfo;
    LLModelType m_llModelType;
    ModelInfo m_modelInfo;
    TokenTimer *m_timer;
    QByteArray m_state;
    QThread m_llmThread;
    std::atomic<bool> m_stopGenerating;
    std::atomic<bool> m_shouldBeLoaded;
    std::atomic<bool> m_isRecalc;
    std::atomic<bool> m_forceUnloadModel;
    std::atomic<bool> m_markedForDeletion;
    bool m_isServer;
    bool m_forceMetal;
    bool m_reloadingToChangeVariant;
    bool m_processedSystemPrompt;
    bool m_restoreStateFromText;
    // m_pristineLoadedState is set if saveSate is unnecessary, either because:
    // - an unload was queued during LLModel::restoreState()
    // - the chat will be restored from text and hasn't been interacted with yet
    bool m_pristineLoadedState = false;
    QVector<QPair<QString, QString>> m_stateFromText;
};

#endif // CHATLLM_H
