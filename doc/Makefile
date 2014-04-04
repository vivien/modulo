all: man html

PROGRAM=modulo

man: $(PROGRAM).1

$(PROGRAM).1: $(PROGRAM).1.ronn
	ronn -w -r $<

html: $(PROGRAM).1.html

$(PROGRAM).1.html: $(PROGRAM).1.ronn
	ronn -w -5 --style toc $<

clean:
	rm -f $(PROGRAM).1 $(PROGRAM).1.html
