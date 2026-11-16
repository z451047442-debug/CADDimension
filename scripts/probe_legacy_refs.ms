-- M0: verify the generated fixture really has NumRefs==2 and ref 1 == LegacyBox.
-- Run with the eb91d0d plugin build (probe_plugin_legacy.ini).

fixture = "D:/PycharmProjects/CADDimension/scripts/fixtures/legacy_b1_nrefs2.max"
resultPath = "D:/PycharmProjects/CADDimension/scripts/fixtures/legacy_probe_result.log"
out = createFile resultPath
format "[LEGACY][START] fixture=%\n" fixture to:out
flush out

resetMaxFile #noPrompt
loadMaxFile fixture

cd = getNodeByName "LegacyDim"
b  = getNodeByName "LegacyBox"
if cd == undefined do throw "legacy fixture: LegacyDim not found"
if b  == undefined do throw "legacy fixture: LegacyBox not found"

obj = cd.baseObject
if obj == undefined do throw "legacy fixture: LegacyDim.baseObject is undefined"

try (
    n = refs.getNumRefs obj
    format "[LEGACY][NUMREFS] %\n" n to:out
    if n != 2 do throw ("legacy fixture: expected NumRefs 2, got " + (n as string))

    for i = 1 to n do (
        r = refs.getReference obj i
        format "[LEGACY][REF] idx=% -> %\n" i (r as string) to:out
    )
    format "[LEGACY][TARGET] %\n" (cd.targetNode as string) to:out
    if cd.targetNode != b do throw "legacy fixture: targetNode is not LegacyBox"

    format "[LEGACY][PASS] fixture is a true NumRefs==2 legacy scene\n" to:out
    close out
) catch (
    format "[LEGACY][FAIL] %\n" (getCurrentException()) to:out
    flush out
    close out
)
