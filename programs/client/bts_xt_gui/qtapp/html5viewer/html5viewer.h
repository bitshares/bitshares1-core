#ifndef HTML5VIEWER_H
#define HTML5VIEWER_H

#include <QWidget>
#include <QUrl>

class QGraphicsWebView;

class Html5Viewer : public QWidget
{
    Q_OBJECT

public:
    enum ScreenOrientation {
        ScreenOrientationLockPortrait,
        ScreenOrientationLockLandscape,
        ScreenOrientationAuto
    };

    explicit Html5Viewer(QWidget *parent = 0);
    virtual ~Html5Viewer();

    void loadFile(const QString &fileName);
    void loadUrl(const QUrl &url);

    // Note that this will only have an effect on Fremantle.
    void setOrientation(ScreenOrientation orientation);

    void showExpanded();

    QGraphicsWebView *webView() const;

private:
    class Html5ViewerPrivate *m_d;
};

#endif 
