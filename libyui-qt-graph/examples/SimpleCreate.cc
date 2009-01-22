
// g++ -I/usr/include/YaST2/yui -I/usr/include/graphviz -lyui -lgvc -lgraph -lcdt SimpleCreate.cc -o SimpleCreate

// Trivial example creating a graph using graphviz functions.


#include <gvc.h>

#include "YUI.h"
#include "YWidgetFactory.h"
#include "YOptionalWidgetFactory.h"
#include "YDialog.h"
#include "YLayoutBox.h"
#include "YEvent.h"
#include "YGraph.h"


int
main(int argc, char** argv)
{
    YDialog* dialog = YUI::widgetFactory()->createPopupDialog();
    YLayoutBox* vbox = YUI::widgetFactory()->createVBox(dialog);

    GVC_t* gvc = gvContext();

    graph_t* graph = agopen("test", AGDIGRAPH);

    node_t* node1 = agnode(graph, "1");
    node_t* node2 = agnode(graph, "2");

    edge_t* edge1 = agedge(graph, node1, node2);
    edge_t* edge2 = agedge(graph, node2, node1);

    gvLayout(gvc, graph, "dot");

    YUI::optionalWidgetFactory()->createGraph(vbox, graph);

    gvFreeLayout(gvc, graph);
    agclose(graph);
    gvFreeContext(gvc);

    dialog->waitForEvent();
    dialog->destroy();
}