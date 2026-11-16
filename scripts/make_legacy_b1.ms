-- Creates a legacy B1 scene (NumRefs=2, P_OWNERS_REF) with the current B1 .dlo.
-- Used by the B1.1 G7 compatibility probe.
resetMaxFile #noPrompt
b = Box length:10 width:20 height:30
b.name = "LegacyBox"
cd = CADDimension()
cd.name = "LegacyDim"
cd.targetNode = b
saveMaxFile "D:/PycharmProjects/CADDimension/scripts/probe_b1_scene.max"
format "[LEGACY][DONE] saved probe_b1_scene.max\n"
