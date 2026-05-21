#include <QtWidgets/QWidget>

namespace Ui
{
    class TestQtOpenglClass;
}

class TestOpengl : public QWidget
{
    Q_OBJECT
public:
    explicit TestOpengl(QWidget *parent = Q_NULLPTR);
    virtual ~TestOpengl();

public:
    Ui::TestQtOpenglClass *ui = nullptr;
};
