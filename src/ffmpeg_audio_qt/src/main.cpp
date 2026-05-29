#include <QtCore/QCoreApplication>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioSink>
#include <QtMultimedia/QMediaDevices>

#include <cstdio>
#include <thread>

int main(int argc, char* argv[])
{
    QCoreApplication a(argc, argv);

    QAudioFormat fmt;
    fmt.setSampleRate(44100);
    fmt.setChannelCount(2);
    fmt.setSampleFormat(QAudioFormat::Int16);

    if (!fmt.isValid())
    {
        qDebug() << "Audio format is invalid!";
        return -1;
    }

    QAudioSink audioSink(QMediaDevices::defaultAudioOutput(), fmt);
    QIODevice* io = audioSink.start();

    constexpr int CHUNK_SIZE = 4096;
    char          buf[CHUNK_SIZE];

    FILE* fp = std::fopen(R"(assert\output.pcm)", "rb");
    if (!fp)
    {
        qDebug() << "Failed to open assert\\output.pcm";
        return -1;
    }

    while (!std::feof(fp))
    {
        if (audioSink.bytesFree() < CHUNK_SIZE)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        const int len = static_cast<int>(std::fread(buf, 1, CHUNK_SIZE, fp));
        if (len <= 0)
        {
            break;
        }

        io->write(buf, len);
    }

    std::fclose(fp);
    return a.exec();
}
