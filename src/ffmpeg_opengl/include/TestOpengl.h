#pragma once

#include <QtWidgets/QWidget>
#include <memory>

namespace Ui
{
class TestQtOpenglClass;
}

class LocalPlayer;

class TestOpengl : public QWidget
{
    Q_OBJECT

public:
    explicit TestOpengl(QWidget* parent = nullptr);
    ~TestOpengl() override;

public:
    Ui::TestQtOpenglClass* ui = nullptr;

private:
    std::unique_ptr<LocalPlayer> player_;
};
