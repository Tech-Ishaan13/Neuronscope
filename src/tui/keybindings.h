#pragma once

namespace Keybindings {
    inline const char* GlobalHelp = 
        "[Tab] Cycle Panel Focus  |  [Q] Quit App  |  [?] Show Help Overlay\n"
        "[1-5] Direct Panel Focus (1: Topology, 2: Packets, 3: Attention, 4: Metrics, 5: Anomalies)";

    inline const char* TopologyHelp =
        "[j/k] Navigate Tree  |  [Space] Expand/Collapse  |  [Enter] Set Capture Target";

    inline const char* AttentionHelp =
        "[Arrows / h,j,k,l] Pan Matrix Viewport  |  [+/-] Scale Contrast  |  [H] Cycle Head  |  [F] Toggle Fullscreen";

    inline const char* GenericHelp =
        "[j/k] Scroll List/Table";
}
