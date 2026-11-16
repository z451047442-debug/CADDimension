-- M0: generate a true NumRefs==2 legacy fixture using the eb91d0d build.
-- Run with: 3dsmaxbatch.exe scripts\make_legacy_fixture.ms -p scripts\probe_plugin_legacy.ini -log scripts\make_legacy_fixture.log -v 4

outPath = "D:/PycharmProjects/CADDimension/scripts/fixtures/legacy_b1_nrefs2.max"

resetMaxFile #noPrompt
b = Box length:10 width:20 height:30
b.name = "LegacyBox"
cd = CADDimension()
cd.name = "LegacyDim"
cd.targetNode = b
saveMaxFile outPath
format "[LEGACY][DONE] saved %\n" outPath
