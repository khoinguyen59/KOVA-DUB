#include "AppErrorCatalog.h"

#include <initializer_list>

namespace LAStudio {

namespace {

bool containsAny(const QString &value, std::initializer_list<QString> needles)
{
    for (const QString &needle : needles) {
        if (value.contains(needle, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

AppErrorPresentation makePresentation(const QString &code,
                                      const QString &source,
                                      const QString &title,
                                      const QString &summary,
                                      const QString &guidance,
                                      const QString &actionId = {},
                                      const QString &actionLabel = {},
                                      const QString &actionRoute = {},
                                      const QString &technicalDetails = {})
{
    AppErrorPresentation presentation;
    presentation.code = code;
    presentation.source = source;
    presentation.title = title;
    presentation.summary = summary;
    presentation.guidance = guidance;
    presentation.actionId = actionId;
    presentation.actionLabel = actionLabel;
    presentation.actionRoute = actionRoute;
    presentation.technicalDetails = technicalDetails;
    return presentation;
}

QString routeForSource(const QString &source)
{
    const QString normalized = source.toLower();
    if (containsAny(normalized, {QStringLiteral("subtitle"), QStringLiteral("ocr")}))
        return QStringLiteral("subtitle-ocr");
    if (containsAny(normalized, {QStringLiteral("stt"), QStringLiteral("speech"), QStringLiteral("transcri")}))
        return QStringLiteral("studio-stt");
    if (containsAny(normalized, {QStringLiteral("tts"), QStringLiteral("voice"), QStringLiteral("synth")}))
        return QStringLiteral("studio-tts");
    if (containsAny(normalized, {QStringLiteral("align")}))
        return QStringLiteral("studio-alignment");
    if (containsAny(normalized, {QStringLiteral("translat")}))
        return QStringLiteral("studio-translation");
    if (containsAny(normalized, {QStringLiteral("llm"), QStringLiteral("chat")}))
        return QStringLiteral("studio-llm");
    if (containsAny(normalized, {QStringLiteral("dubbing")}))
        return QStringLiteral("studio-dubbing");
    if (containsAny(normalized, {QStringLiteral("isolat"), QStringLiteral("separat")}))
        return QStringLiteral("studio-voice-isolator");
    return {};
}

AppErrorPresentation featureFailure(const QString &source,
                                    const QString &route,
                                    const QString &featureName,
                                    const QString &code,
                                    const QString &technicalDetails)
{
    const QString actionLabel = QStringLiteral("Mở cấu hình %1").arg(featureName);
    return makePresentation(
        code,
        source,
        QStringLiteral("%1 chưa hoàn tất").arg(featureName),
        QStringLiteral("LA Studio không thể hoàn thành bước này với cấu hình hiện tại."),
        QStringLiteral("1. Mở cấu hình của bước này và kiểm tra model/runtime hoặc worker đang được chọn.\n"
                       "2. Kiểm tra tệp đầu vào và quyền truy cập thư mục.\n"
                       "3. Sửa cấu hình rồi chạy lại bước; nếu vẫn lỗi, tạo báo cáo hỗ trợ để gửi cùng log kỹ thuật."),
        QStringLiteral("open-feature"),
        actionLabel,
        route,
        technicalDetails);
}

} // namespace

QVariantMap AppErrorPresentation::toVariantMap() const
{
    QVariantMap result;
    result.insert(QStringLiteral("code"), code);
    result.insert(QStringLiteral("severity"), QStringLiteral("error"));
    result.insert(QStringLiteral("source"), source);
    result.insert(QStringLiteral("title"), title);
    result.insert(QStringLiteral("summary"), summary);
    result.insert(QStringLiteral("guidance"), guidance);
    result.insert(QStringLiteral("actionId"), actionId);
    result.insert(QStringLiteral("actionLabel"), actionLabel);
    result.insert(QStringLiteral("actionRoute"), actionRoute);
    // Keep both names: message is the existing compatibility field and
    // technicalDetails is the explicit UI/diagnostics contract.
    result.insert(QStringLiteral("message"), technicalDetails);
    result.insert(QStringLiteral("technicalDetails"), technicalDetails);
    return result;
}

AppErrorPresentation classifyAppError(const QString &technicalMessage, const QString &source)
{
    const QString message = technicalMessage.trimmed();
    const QString normalizedMessage = message.toLower();
    const QString normalizedSource = source.trimmed().toLower();
    const QString context = normalizedSource + QLatin1Char(' ') + normalizedMessage;
    const QString separationRoute = normalizedSource.contains(QStringLiteral("dubbing"))
        ? QStringLiteral("studio-dubbing")
        : QStringLiteral("studio-voice-isolator");

    const bool separationContext = containsAny(context, {
        QStringLiteral("voice isolation"), QStringLiteral("voice isolator"),
        QStringLiteral("source separation"), QStringLiteral("demucs"),
        QStringLiteral("mdx"), QStringLiteral("spleeter"), QStringLiteral("separat"),
        QStringLiteral("isolat")
    });
    const bool runtimeOrModelUnavailable = containsAny(normalizedMessage, {
        QStringLiteral("runtime"), QStringLiteral("model"), QStringLiteral("backend"),
        QStringLiteral("unavailable"), QStringLiteral("not found"), QStringLiteral("missing"),
        QStringLiteral("not configured"), QStringLiteral("configure")
    });

    if (separationContext && runtimeOrModelUnavailable) {
        return makePresentation(
            QStringLiteral("dubbing-separation-runtime"),
            source,
            QStringLiteral("Chưa thể tách giọng"),
            QStringLiteral("Runtime hoặc model tách giọng chưa sẵn sàng cho bước này."),
            QStringLiteral("1. Mở cấu hình tách giọng và chọn đúng model/runtime đã cài đặt.\n"
                           "2. Nếu dùng Colab, kết nối đúng worker GPU cho tác vụ tách giọng.\n"
                           "3. Kiểm tra tệp âm thanh nguồn, sau đó chạy lại bước tách giọng.\n"
                           "4. Nếu lỗi lặp lại, tạo báo cáo hỗ trợ để gửi kèm log kỹ thuật."),
            QStringLiteral("open-separation"),
            QStringLiteral("Mở cấu hình tách giọng"),
            separationRoute,
            message);
    }

    if (separationContext) {
        return featureFailure(source,
                              separationRoute,
                              QStringLiteral("tách giọng"),
                              QStringLiteral("voice-isolation-failure"),
                              message);
    }

    const QString sourceRoute = routeForSource(source);
    if (!sourceRoute.isEmpty()) {
        QString featureName;
        QString code;
        if (sourceRoute == QStringLiteral("studio-stt")) {
            featureName = QStringLiteral("nhận dạng lời thoại");
            code = QStringLiteral("stt-failure");
        } else if (sourceRoute == QStringLiteral("studio-tts")) {
            featureName = QStringLiteral("tổng hợp giọng nói");
            code = QStringLiteral("tts-failure");
        } else if (sourceRoute == QStringLiteral("studio-alignment")) {
            featureName = QStringLiteral("căn chỉnh transcript");
            code = QStringLiteral("alignment-failure");
        } else if (sourceRoute == QStringLiteral("studio-translation")) {
            featureName = QStringLiteral("dịch");
            code = QStringLiteral("translation-failure");
        } else if (sourceRoute == QStringLiteral("studio-llm")) {
            featureName = QStringLiteral("LLM");
            code = QStringLiteral("llm-failure");
        } else if (sourceRoute == QStringLiteral("subtitle-ocr")) {
            featureName = QStringLiteral("OCR phụ đề");
            code = QStringLiteral("subtitle-ocr-failure");
        } else {
            featureName = QStringLiteral("Dubbing");
            code = QStringLiteral("dubbing-failure");
        }
        return featureFailure(source, sourceRoute, featureName, code, message);
    }

    const bool fileFailure = containsAny(normalizedMessage, {
        QStringLiteral("file"), QStringLiteral("path"), QStringLiteral("artifact"),
        QStringLiteral("permission"), QStringLiteral("access denied"), QStringLiteral("cannot open")
    });
    if (fileFailure) {
        return makePresentation(
            QStringLiteral("file-or-artifact-failure"),
            source,
            QStringLiteral("Không đọc được tệp kết quả"),
            QStringLiteral("Tệp đầu vào hoặc output cần thiết không tồn tại, không hợp lệ hoặc không thể truy cập."),
            QStringLiteral("1. Kiểm tra lại đường dẫn và giữ nguyên tệp trong suốt quá trình chạy.\n"
                           "2. Chọn lại đúng file output nếu bạn đang handoff từ worker bên ngoài.\n"
                           "3. Kiểm tra quyền đọc/ghi thư mục rồi thử lại bước hiện tại."),
            QStringLiteral("check-file"),
            QStringLiteral("Kiểm tra tệp đầu vào"),
            {},
            message);
    }

    const bool connectionFailure = containsAny(normalizedMessage, {
        QStringLiteral("network"), QStringLiteral("connection"), QStringLiteral("timeout"),
        QStringLiteral("authentication"), QStringLiteral("unauthorized"), QStringLiteral("401"),
        QStringLiteral("403"), QStringLiteral("gateway"), QStringLiteral("colab")
    });
    if (connectionFailure) {
        return makePresentation(
            QStringLiteral("service-connection-failure"),
            source,
            QStringLiteral("Không thể kết nối dịch vụ"),
            QStringLiteral("Dịch vụ hoặc worker bên ngoài chưa phản hồi đúng cách."),
            QStringLiteral("1. Kiểm tra kết nối mạng và trạng thái worker.\n"
                           "2. Xác nhận phiên Colab/API còn hoạt động và đúng quyền truy cập.\n"
                           "3. Chờ vài giây rồi thử lại; không đóng ứng dụng khi worker còn đang xử lý."),
            QStringLiteral("retry-connection"),
            QStringLiteral("Kiểm tra kết nối"),
            sourceRoute,
            message);
    }

    return makePresentation(
        QStringLiteral("unknown-error"),
        source,
        QStringLiteral("Tác vụ chưa hoàn tất"),
        QStringLiteral("LA Studio đã ghi lại lỗi kỹ thuật nhưng chưa xác định được nguyên nhân tự động."),
        QStringLiteral("1. Kiểm tra lại cấu hình và tệp đầu vào của bước hiện tại.\n"
                       "2. Thử lại một lần sau khi lưu công việc hiện tại.\n"
                       "3. Nếu lỗi lặp lại, bấm “Tạo báo cáo hỗ trợ” và gửi báo cáo cùng mô tả thao tác vừa thực hiện."),
        QStringLiteral("create-report"),
        QStringLiteral("Tạo báo cáo hỗ trợ"),
        {},
        message);
}

} // namespace LAStudio
