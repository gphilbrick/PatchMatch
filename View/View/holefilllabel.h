#ifndef VIEW_HOLEFILLLABEL_H
#define VIEW_HOLEFILLLABEL_H

#include <CoreQt/imageeditlabel.h>

#include <Core/image/imagetypes.h>
#include <Core/utility/twodarray.h>
#include <Core/utility/vector4.h>

namespace view {

class HoleFillLabel : public coreQt::ImageEditLabel
{
    Q_OBJECT
public:
    HoleFillLabel( QWidget* parent = 0 );
    /// Indefinitely show/hide the hole in progress.
    void toggleHoleVisibility( bool );
    /// Make the hole invisible until 'this' processes an input event.
    void brieflyHideHole();
    void clearHole();
    const core::ImageBinary& hole() const;
    void setHole( const core::ImageBinary& );
public slots:
    void brushWidthSlot(int);
    void clearHoleSlot();
protected:
    void mouseMoveEvent(QMouseEvent *ev) override;
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void wheelEvent(QWheelEvent* event) override;
    void loadEvent(int) override;
private:
    enum class HoleMode
    {
        Hidden,
        HiddenBriefly,
        Visible
    };

    void applyBrush(const core::IntCoord& imageCoord);

    core::ImageBinary _hole;
    core::ImageRGBA _holeDisplay;

    int _brushWidth;
    bool _brushDown;
    HoleMode _holeMode;
};

} // view

#endif // #include
