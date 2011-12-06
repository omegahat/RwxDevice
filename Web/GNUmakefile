PKG_NAME=$(shell grep Package ../DESCRIPTION | sed -e 's/.*: //')

index.html: index.html.in configure
	./configure

include $(OMEGA_HOME)/R/Config/RPackageWebConfig

Rexif.pdf:
	R CMD Rd2dvi -o Rexif.pdf --pdf ../../Rexif

#html
indexInstall: index.html
	scp index.html $(TARGET_DIR)

configure: configure.in
	autoconf
	chmod +x $@