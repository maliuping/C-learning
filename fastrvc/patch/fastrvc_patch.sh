git am 0005-fastrvc-modidfy.patch
git am 0006-fastrvc-modify-logo-path-in-xml.patch
git am 0007-fastrvc-modify-scan-rule-for-proc-dir.patch
cd ../../../../../kernel
git am ../vendor/mediatek/mt2712/fastRvc/patch/kernel/0001-fastrvc-modify-init-process-to-initforfastrvc.patch
cd ../external/libxml2
git am ../../vendor/mediatek/mt2712/fastRvc/patch/external/libxml2/0001-xml2-modify-xml2-option-for-fastrvc.patch
cd ../../device/mediatek/mt2712
git am ../../../vendor/mediatek/mt2712/fastRvc/patch/device/mediatek/mt2712/0001-fastrvc-add-fastrvc-selinux-initflow.patch