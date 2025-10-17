@echo off
setlocal

:: 修改成你 Tesseract 的安装目录
set TESSERACT_PATH=D:\\Program Files\\Tesseract-OCR
set PATH=%TESSERACT_PATH%;%PATH%

echo === Step 1: 生成 .tr 文件 ===
for %%f in (*.png) do (
    echo 正在处理 %%f ...
    tesseract %%f %%~nf nobatch box.train
)

echo === Step 2: 提取字符集 ===
for %%f in (*.box) do (
    echo 正在处理 %%f ...
    unicharset_extractor %%f 
)

echo === Step 3: shapeclustering ===
for %%f in (*.tr) do (
    echo 正在处理 %%f ...
    shapeclustering -F font_properties -U unicharset %%f 
)

echo === Step 4: mftraining ===
for %%f in (*.tr) do (
    echo 正在处理 %%f ...
    mftraining -F font_properties -U unicharset -O chi_sim_custom.unicharset %%f 
)

echo === Step 5: cntraining ===
for %%f in (*.tr) do (
    echo 正在处理 %%f ...
    cntraining %%f 
)
combine_tessdata chi_sim_custom
echo === Step 6: 生成语言文件 ===
rename inttemp chi_sim_custom.inttemp
rename normproto chi_sim_custom.normproto
rename pffmtable chi_sim_custom.pffmtable
rename shapetable chi_sim_custom.shapetable
copy /Y unicharset chi_sim_custom.unicharset

echo === Step 7: 拷贝到 tessdata ===
for %%f in (chi_sim_custom.*) do (
    echo 正在处理 %%f ...
    cntraining %%f 
	copy /Y %%f %TESSERACT_PATH%\tessdata\
)

echo.
echo ✅ 训练完成! 可用 -l chi_sim_custom 测试
pause
