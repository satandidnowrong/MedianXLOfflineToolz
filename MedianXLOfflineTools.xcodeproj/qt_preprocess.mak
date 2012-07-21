#############################################################################
# Makefile for building: MedianXLOfflineTools.app/Contents/MacOS/MedianXLOfflineTools
# Generated by qmake (2.01a) (Qt 4.8.2) on: Sun Jun 10 00:33:48 2012
# Project:  MedianXLOfflineTools.pro
# Template: app
# Command: /usr/bin/qmake -spec /usr/local/Qt4.8/mkspecs/macx-xcode -o MedianXLOfflineTools.xcodeproj/project.pbxproj MedianXLOfflineTools.pro
#############################################################################

MOC       = /Developer/Tools/Qt/moc
UIC       = /Developer/Tools/Qt/uic
LEX       = flex
LEXFLAGS  = 
YACC      = yacc
YACCFLAGS = -d
DEFINES       = -DQT_GUI_LIB -DQT_NETWORK_LIB -DQT_CORE_LIB -DQT_SHARED
INCPATH       = -I/usr/local/Qt4.8/mkspecs/macx-xcode -I. -I/Library/Frameworks/QtCore.framework/Versions/4/Headers -I/usr/include/QtCore -I/Library/Frameworks/QtNetwork.framework/Versions/4/Headers -I/usr/include/QtNetwork -I/Library/Frameworks/QtGui.framework/Versions/4/Headers -I/usr/include/QtGui -I/usr/include -I. -I. -I/usr/local/include -I/System/Library/Frameworks/CarbonCore.framework/Headers -F/Library/Frameworks
DEL_FILE  = rm -f
MOVE      = mv -f

IMAGES = 
PARSERS =
preprocess: $(PARSERS) compilers
clean preprocess_clean: parser_clean compiler_clean

parser_clean:
check: first

mocclean: compiler_moc_header_clean compiler_moc_source_clean

mocables: compiler_moc_header_make_all compiler_moc_source_make_all

compilers: ./application_mac.o ./messagecheckbox_mac_p.o ./medianxlofflinetools_mac.o\
	 ./machelpers.o ./fileassociationmanager_mac.o ./moc_medianxlofflinetools.cpp ./moc_resurrectpenaltydialog.cpp ./moc_qd2charrenamer.cpp\
	 ./moc_enums.cpp ./moc_propertiesviewerwidget.cpp ./moc_itemsviewerdialog.cpp\
	 ./moc_itemstoragetablemodel.cpp ./moc_itemstoragetableview.cpp ./moc_itemspropertiessplitter.cpp\
	 ./moc_finditemsdialog.cpp ./moc_findresultswidget.cpp ./moc_skillplandialog.cpp\
	 ./moc_application.cpp ./moc_qtsingleapplication.cpp ./moc_qtlocalpeer.cpp\
	 ./moc_experienceindicatorgroupbox.cpp ./qrc_medianxlofflinetools.cpp ./ui_medianxlofflinetools.h ./ui_resurrectpenaltydialog.h ./ui_qd2charrenamer.h\
	 ./ui_propertiesviewerwidget.h ./ui_finditemsdialog.h ./ui_skillplandialog.h
compiler_objective_c_make_all: application_mac.o messagecheckbox_mac_p.o medianxlofflinetools_mac.o machelpers.o fileassociationmanager_mac.o
compiler_objective_c_clean:
	-$(DEL_FILE) application_mac.o messagecheckbox_mac_p.o medianxlofflinetools_mac.o machelpers.o fileassociationmanager_mac.o
application_mac.o: application.h \
		qtsingleapplication/qtsingleapplication.h \
		medianxlofflinetools.h \
		ui_medianxlofflinetools.h \
		enums.h \
		helpers.h \
		colorsmanager.hpp \
		resurrectpenaltydialog.h \
		ui_resurrectpenaltydialog.h \
		structs.h \
		application_mac.mm
	gcc -c $(QMAKE_COMP_QMAKE_OBJECTIVE_CFLAGS) $(DEFINES) $(INCPATH) application_mac.mm -o application_mac.o

messagecheckbox_mac_p.o: messagecheckbox.h \
		machelpers.h \
		messagecheckbox_mac_p.mm
	gcc -c $(QMAKE_COMP_QMAKE_OBJECTIVE_CFLAGS) $(DEFINES) $(INCPATH) messagecheckbox_mac_p.mm -o messagecheckbox_mac_p.o

medianxlofflinetools_mac.o: medianxlofflinetools.h \
		ui_medianxlofflinetools.h \
		enums.h \
		helpers.h \
		colorsmanager.hpp \
		resurrectpenaltydialog.h \
		ui_resurrectpenaltydialog.h \
		structs.h \
		machelpers.h \
		medianxlofflinetools_mac.mm
	gcc -c $(QMAKE_COMP_QMAKE_OBJECTIVE_CFLAGS) $(DEFINES) $(INCPATH) medianxlofflinetools_mac.mm -o medianxlofflinetools_mac.o

machelpers.o: machelpers.mm
	gcc -c $(QMAKE_COMP_QMAKE_OBJECTIVE_CFLAGS) $(DEFINES) $(INCPATH) machelpers.mm -o machelpers.o

fileassociationmanager_mac.o: fileassociationmanager.h \
		helpers.h \
		colorsmanager.hpp \
		machelpers.h \
		fileassociationmanager_mac.mm
	gcc -c $(QMAKE_COMP_QMAKE_OBJECTIVE_CFLAGS) $(DEFINES) $(INCPATH) fileassociationmanager_mac.mm -o fileassociationmanager_mac.o

compiler_moc_header_make_all: moc_medianxlofflinetools.cpp moc_resurrectpenaltydialog.cpp moc_qd2charrenamer.cpp moc_enums.cpp moc_propertiesviewerwidget.cpp moc_itemsviewerdialog.cpp moc_itemstoragetablemodel.cpp moc_itemstoragetableview.cpp moc_itemspropertiessplitter.cpp moc_finditemsdialog.cpp moc_findresultswidget.cpp moc_skillplandialog.cpp moc_application.cpp moc_qtsingleapplication.cpp moc_qtlocalpeer.cpp moc_experienceindicatorgroupbox.cpp
compiler_moc_header_clean:
	-$(DEL_FILE) moc_medianxlofflinetools.cpp moc_resurrectpenaltydialog.cpp moc_qd2charrenamer.cpp moc_enums.cpp moc_propertiesviewerwidget.cpp moc_itemsviewerdialog.cpp moc_itemstoragetablemodel.cpp moc_itemstoragetableview.cpp moc_itemspropertiessplitter.cpp moc_finditemsdialog.cpp moc_findresultswidget.cpp moc_skillplandialog.cpp moc_application.cpp moc_qtsingleapplication.cpp moc_qtlocalpeer.cpp moc_experienceindicatorgroupbox.cpp
moc_medianxlofflinetools.cpp: ui_medianxlofflinetools.h \
		enums.h \
		helpers.h \
		colorsmanager.hpp \
		resurrectpenaltydialog.h \
		ui_resurrectpenaltydialog.h \
		structs.h \
		medianxlofflinetools.h
	/Developer/Tools/Qt/moc $(DEFINES) $(INCPATH) -D__APPLE__ -D__GNUC__ medianxlofflinetools.h -o moc_medianxlofflinetools.cpp

moc_resurrectpenaltydialog.cpp: ui_resurrectpenaltydialog.h \
		resurrectpenaltydialog.h
	/Developer/Tools/Qt/moc $(DEFINES) $(INCPATH) -D__APPLE__ -D__GNUC__ resurrectpenaltydialog.h -o moc_resurrectpenaltydialog.cpp

moc_qd2charrenamer.cpp: ui_qd2charrenamer.h \
		qd2charrenamer.h
	/Developer/Tools/Qt/moc $(DEFINES) $(INCPATH) -D__APPLE__ -D__GNUC__ qd2charrenamer.h -o moc_qd2charrenamer.cpp

moc_enums.cpp: helpers.h \
		colorsmanager.hpp \
		enums.h
	/Developer/Tools/Qt/moc $(DEFINES) $(INCPATH) -D__APPLE__ -D__GNUC__ enums.h -o moc_enums.cpp

moc_propertiesviewerwidget.cpp: ui_propertiesviewerwidget.h \
		structs.h \
		enums.h \
		helpers.h \
		colorsmanager.hpp \
		propertiesviewerwidget.h
	/Developer/Tools/Qt/moc $(DEFINES) $(INCPATH) -D__APPLE__ -D__GNUC__ propertiesviewerwidget.h -o moc_propertiesviewerwidget.cpp

moc_itemsviewerdialog.cpp: structs.h \
		enums.h \
		helpers.h \
		colorsmanager.hpp \
		itemsviewerdialog.h
	/Developer/Tools/Qt/moc $(DEFINES) $(INCPATH) -D__APPLE__ -D__GNUC__ itemsviewerdialog.h -o moc_itemsviewerdialog.cpp

moc_itemstoragetablemodel.cpp: structs.h \
		enums.h \
		helpers.h \
		colorsmanager.hpp \
		itemstoragetablemodel.h
	/Developer/Tools/Qt/moc $(DEFINES) $(INCPATH) -D__APPLE__ -D__GNUC__ itemstoragetablemodel.h -o moc_itemstoragetablemodel.cpp

moc_itemstoragetableview.cpp: itemstoragetableview.h
	/Developer/Tools/Qt/moc $(DEFINES) $(INCPATH) -D__APPLE__ -D__GNUC__ itemstoragetableview.h -o moc_itemstoragetableview.cpp

moc_itemspropertiessplitter.cpp: structs.h \
		enums.h \
		helpers.h \
		colorsmanager.hpp \
		itemspropertiessplitter.h
	/Developer/Tools/Qt/moc $(DEFINES) $(INCPATH) -D__APPLE__ -D__GNUC__ itemspropertiessplitter.h -o moc_itemspropertiessplitter.cpp

moc_finditemsdialog.cpp: ui_finditemsdialog.h \
		findresultswidget.h \
		structs.h \
		enums.h \
		helpers.h \
		colorsmanager.hpp \
		finditemsdialog.h
	/Developer/Tools/Qt/moc $(DEFINES) $(INCPATH) -D__APPLE__ -D__GNUC__ finditemsdialog.h -o moc_finditemsdialog.cpp

moc_findresultswidget.cpp: structs.h \
		enums.h \
		helpers.h \
		colorsmanager.hpp \
		findresultswidget.h
	/Developer/Tools/Qt/moc $(DEFINES) $(INCPATH) -D__APPLE__ -D__GNUC__ findresultswidget.h -o moc_findresultswidget.cpp

moc_skillplandialog.cpp: ui_skillplandialog.h \
		skillplandialog.h
	/Developer/Tools/Qt/moc $(DEFINES) $(INCPATH) -D__APPLE__ -D__GNUC__ skillplandialog.h -o moc_skillplandialog.cpp

moc_application.cpp: qtsingleapplication/qtsingleapplication.h \
		application.h
	/Developer/Tools/Qt/moc $(DEFINES) $(INCPATH) -D__APPLE__ -D__GNUC__ application.h -o moc_application.cpp

moc_qtsingleapplication.cpp: qtsingleapplication/qtsingleapplication.h
	/Developer/Tools/Qt/moc $(DEFINES) $(INCPATH) -D__APPLE__ -D__GNUC__ qtsingleapplication/qtsingleapplication.h -o moc_qtsingleapplication.cpp

moc_qtlocalpeer.cpp: qtsingleapplication/qtlockedfile.h \
		qtsingleapplication/qtlocalpeer.h
	/Developer/Tools/Qt/moc $(DEFINES) $(INCPATH) -D__APPLE__ -D__GNUC__ qtsingleapplication/qtlocalpeer.h -o moc_qtlocalpeer.cpp

moc_experienceindicatorgroupbox.cpp: experienceindicatorgroupbox.h
	/Developer/Tools/Qt/moc $(DEFINES) $(INCPATH) -D__APPLE__ -D__GNUC__ experienceindicatorgroupbox.h -o moc_experienceindicatorgroupbox.cpp

compiler_rcc_make_all: qrc_medianxlofflinetools.cpp
compiler_rcc_clean:
	-$(DEL_FILE) qrc_medianxlofflinetools.cpp
qrc_medianxlofflinetools.cpp: resources/medianxlofflinetools.qrc
	/Developer/Tools/Qt/rcc -name medianxlofflinetools resources/medianxlofflinetools.qrc -o qrc_medianxlofflinetools.cpp

compiler_image_collection_make_all: qmake_image_collection.cpp
compiler_image_collection_clean:
	-$(DEL_FILE) qmake_image_collection.cpp
compiler_moc_source_make_all:
compiler_moc_source_clean:
compiler_rez_source_make_all:
compiler_rez_source_clean:
compiler_uic_make_all: ui_medianxlofflinetools.h ui_resurrectpenaltydialog.h ui_qd2charrenamer.h ui_propertiesviewerwidget.h ui_finditemsdialog.h ui_skillplandialog.h
compiler_uic_clean:
	-$(DEL_FILE) ui_medianxlofflinetools.h ui_resurrectpenaltydialog.h ui_qd2charrenamer.h ui_propertiesviewerwidget.h ui_finditemsdialog.h ui_skillplandialog.h
ui_medianxlofflinetools.h: medianxlofflinetools.ui
	/Developer/Tools/Qt/uic medianxlofflinetools.ui -o ui_medianxlofflinetools.h

ui_resurrectpenaltydialog.h: resurrectpenaltydialog.ui
	/Developer/Tools/Qt/uic resurrectpenaltydialog.ui -o ui_resurrectpenaltydialog.h

ui_qd2charrenamer.h: qd2charrenamer.ui
	/Developer/Tools/Qt/uic qd2charrenamer.ui -o ui_qd2charrenamer.h

ui_propertiesviewerwidget.h: propertiesviewerwidget.ui
	/Developer/Tools/Qt/uic propertiesviewerwidget.ui -o ui_propertiesviewerwidget.h

ui_finditemsdialog.h: finditemsdialog.ui
	/Developer/Tools/Qt/uic finditemsdialog.ui -o ui_finditemsdialog.h

ui_skillplandialog.h: skillplandialog.ui
	/Developer/Tools/Qt/uic skillplandialog.ui -o ui_skillplandialog.h

compiler_yacc_decl_make_all:
compiler_yacc_decl_clean:
compiler_yacc_impl_make_all:
compiler_yacc_impl_clean:
compiler_lex_make_all:
compiler_lex_clean:
compiler_clean: compiler_objective_c_clean compiler_moc_header_clean compiler_rcc_clean compiler_uic_clean 
