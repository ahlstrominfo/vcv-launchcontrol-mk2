#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
    pluginInstance = p;

    // Add modules here
    p->addModel(modelCore);
    p->addModel(modelKnobExpander);
    p->addModel(modelGateExpander);
    p->addModel(modelSeqExpander);
    p->addModel(modelClockExpander);
}
