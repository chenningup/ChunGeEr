@echo off
setlocal


echo === 批量生成初始 box 文件 ===
for %%f in (*.png) do (
    echo 正在处理 %%f ...
    tesseract %%f %%~nf -l chi_sim --psm 6 batch.nochop makebox
)

echo.
echo ✅ 所有 box 文件已生成，可以用 jTessBoxEditor 打开修正
pause
