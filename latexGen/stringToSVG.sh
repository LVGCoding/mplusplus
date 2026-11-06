#!/bin/bash
tex="../latexGen/newLatex.tex"
dvi="../latexGen/newLatex.dvi"
svg="../latexGen/newSVG.svg"


latex -output-directory=../latexGen -halt-on-error -interaction=nonstopmode "$tex"
dvisvgm --no-fonts "$dvi" -o "$svg"