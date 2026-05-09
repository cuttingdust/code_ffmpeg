#include <QtCore/QCoreApplication>
#include <QtCore/QThread>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioSink>
#include <QtMultimedia/QMediaDevices>

#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    /// ========== 1. 配置音频格式 (Qt 6 新版 API) ==========
    QAudioFormat fmt;
    fmt.setSampleRate(44100);
    fmt.setChannelCount(2);
    ///  Qt 6: 使用 setSampleFormat 一次性指定类型和位深
    ///    注意：你的原始数据是 UnSignedInt，这里改为 SignedInt
    ///    如果确实需要无符号，可以用 QAudioFormat::UInt8 (但16位无符号不常见)
    fmt.setSampleFormat(QAudioFormat::Int16); /// 对应 s16le 格式

    /// ❌ Qt 6: 以下函数已移除，直接删除
    /// fmt.setSampleSize(16);
    /// fmt.setCodec("audio/pcm");
    /// fmt.setByteOrder(QAudioFormat::LittleEndian);
    /// fmt.setSampleType(QAudioFormat::UnSignedInt);

    /// 检查格式是否有效
    if (!fmt.isValid())
    {
        qDebug() << "Audio format is invalid!";
        return -1;
    }

    /// ========== 2. 创建音频输出 (Qt 6: QAudioSink) ==========
    /// Qt 6: QAudioOutput → QAudioSink
    ///      需要传入 QAudioDevice，nullptr 表示使用默认设备
    QAudioSink *audioSink = new QAudioSink(QMediaDevices::defaultAudioOutput(), fmt);

    /// ========== 3. 开始播放 ==========
    QIODevice *io = audioSink->start(); /// 这个 API 保持不变

    const int CHUNK_SIZE = 4096; /// 每次读取/写入 4096 字节
    char     *buf        = new char[CHUNK_SIZE];

    FILE *fp = fopen(R"(assert\output.pcm)", "rb");
    while (!feof(fp))
    {
        /// bytesFree() 仍然可用
        if (audioSink->bytesFree() < CHUNK_SIZE)
        {
            // QThread::msleep(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        int len = fread(buf, 1, CHUNK_SIZE, fp);
        if (len <= 0)
        {
            break;
        }

        io->write(buf, len);
    }
    fclose(fp);
    delete[] buf;
    buf = nullptr;


    return a.exec();
}
