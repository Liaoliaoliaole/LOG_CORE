SRC_dir=Doc_src
BUILD_dir=build-doc
WORK_dir=work-doc
Latex_FLAGS=-pdf -pdflatex="pdflatex -interaction=nonstopmode" -use-make -outdir=$(WORK_dir)


all: $(BUILD_dir)/Morfeas_RPi_Hat.pdf

#Compiling of the document
$(BUILD_dir)/Morfeas_RPi_Hat.pdf: $(SRC_dir)/*.tex
	latexmk $(Latex_FLAGS) $(SRC_dir)/Morfeas_RPi_Hat.tex && mv $(WORK_dir)/*.pdf $(BUILD_dir)/

tree:
	mkdir -p $(BUILD_dir) $(WORK_dir)

delete-the-tree:
	rm -f -r $(WORK_dir) $(BUILD_dir)

clean:
	rm -f $(WORK_dir)/* $(BUILD_dir)/*

.PHONY: all clean clean-tree tree


