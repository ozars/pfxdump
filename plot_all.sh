#!/bin/sh
python3 plot.py csv ~/andes/src/zidxbenchmark/experiment/exp_remote1.csv results_remote
python3 plot.py csv ~/andes/src/zidxbenchmark/experiment/exp3.csv results_local
python3 plot.py zx data results
python3 gunzip_plot.py
for f in `ls -1 *.eps`; do epstopdf $f; pdfcrop ${f%.*}.pdf; pdftops -eps ${f%.*}-crop.pdf $f; rm -f ${f%.*}.pdf ${f%.*}-crop.pdf; done
