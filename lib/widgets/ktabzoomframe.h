#ifndef __K_TABZOOMFRAME_H__
#define __K_TABZOOMFRAME_H__


#include <qframe.h>


#include "ktabzoomposition.h"


class KTabZoomFramePrivate;

class KTabZoomFrame : public QWidget
{
  Q_OBJECT

public:

  KTabZoomFrame(QWidget *parent=0, KTabZoomPosition::Position pos=KTabZoomPosition::Left, const char *name=0);
  ~KTabZoomFrame();

  int addTab(QWidget *widget, const QString &title);
  void removeTab(int index);


public slots:

  void selected(int index);

  
protected:

  void mousePressEvent(QMouseEvent *ev);
  void mouseReleaseEvent(QMouseEvent *ev);
  void mouseMoveEvent(QMouseEvent *ev);
  

private:

  KTabZoomFramePrivate *d;

};


#endif
