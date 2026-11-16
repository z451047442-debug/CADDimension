-- P0: generate a B4d (NumRefs==1, bbox mode) fixture with the 25884bb build.
resetMaxFile #noPrompt
b = Box length:10 width:20 height:30
b.name = "B4dBox"
d = CADDimension()
d.name = "B4dDim"
d.targetNode = b
out = "D:/PycharmProjects/CADDimension/scripts/fixtures/b4d_bbox_nrefs1.max"
saveMaxFile out
format "[B4D][DONE] %\n" out
