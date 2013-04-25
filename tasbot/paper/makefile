
default: paper.pdf

paper.pdf : paper.tex paper.bib *.png
	pdflatex paper
	bibtex paper
	pdflatex paper
	pdflatex paper

clean :
	rm -f paper.pdf *.log *.aux *.bbl *.blg *.out
