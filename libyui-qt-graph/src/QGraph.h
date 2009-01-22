/*---------------------------------------------------------------------\
|								       |
|		       __   __	  ____ _____ ____		       |
|		       \ \ / /_ _/ ___|_   _|___ \		       |
|			\ V / _` \___ \ | |   __) |		       |
|			 | | (_| |___) || |  / __/		       |
|			 |_|\__,_|____/ |_| |_____|		       |
|								       |
|				core system			       |
|						   (c) SuSE Linux GmbH |
\----------------------------------------------------------------------/

  File:		QGraph.h

  Author:	Arvin Schnell <aschnell@suse.de>

/-*/


#ifndef QGraph_h
#define QGraph_h

#include <gvc.h>
#include <string>

#include <QKeyEvent>
#include <QWheelEvent>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QPicture>


class QGraph : public QGraphicsView
{
    Q_OBJECT

public:

    QGraph(QWidget* parent, const std::string& filename, const std::string& layoutAlgorithm);

    QGraph(QWidget* parent, graph_t* graph);

    virtual ~QGraph();

    virtual void renderGraph(const std::string& filename, const std::string& layoutAlgorithm);
    virtual void renderGraph(graph_t* graph);

protected:

    void keyPressEvent(QKeyEvent* event);
    void wheelEvent(QWheelEvent* event);
    void scaleView(qreal scaleFactor);

private:

    void init();

    QGraphicsScene* scene;

    QSizeF size;

    /**
     * Transform graphviz point into Qt QPointF.
     */
    QPointF gToQ(const point& p, bool trans) const;

    /**
     * Transform graphviz pointf into Qt QPointF.
     */
    QPointF gToQ(const pointf& p, bool trans) const;

    QString aggetToQString(void* obj, const char* name, const char* fallback) const;

    QColor aggetToQColor(void* obj, const char* name, const QColor& fallback) const;

    Qt::PenStyle aggetToQPenStyle(void* obj, const char* name, const Qt::PenStyle fallback) const;

    void arrow(QPainterPath& path, const QLineF& line, const QString& type) const;

    QPolygonF haha1(node_t* node) const;

    QPainterPath haha2(node_t* node) const;

    void drawLabel(const textlabel_t* textlabel, QPainter* painter) const;

};


class Node : public QGraphicsPathItem
{

public:

    Node(const QPainterPath& path);

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

    QPicture picture;

};


#endif // QGraph_h