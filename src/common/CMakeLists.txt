add_library(projectM-Qt-Common STATIC
        application.qrc
        configfile.cpp
        configfile.hpp
        nullable.hpp
        qplaylistfiledialog.cpp
        qplaylistfiledialog.hpp
        qplaylistmodel.cpp
        qplaylistmodel.hpp
        qplaylisttableview.hpp
        qpreseteditordialog.cpp
        qpreseteditordialog.hpp
        qpreseteditordialog.ui
        qpresetfiledialog.hpp
        qpresettextedit.cpp
        qpresettextedit.hpp
        qprojectm.hpp
        qprojectm_mainwindow.cpp
        qprojectm_mainwindow.hpp
        qprojectm_mainwindow.ui
        qprojectmconfigdialog.cpp
        qprojectmconfigdialog.hpp
        qprojectmconfigdialog.ui
        qprojectmwidget.hpp
        qxmlplaylisthandler.hpp
        )

set_target_properties(projectM-Qt-Common PROPERTIES
        C_VISIBILITY_PRESET default
        CXX_VISIBILITY_PRESET default
        VISIBILITY_INLINES_HIDDEN OFF
        )

target_include_directories(projectM-Qt-Common
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        )

target_link_libraries(projectM-Qt-Common
        PUBLIC
        libprojectM::$<IF:$<BOOL:ENABLE_SHARED_LINKING>,shared,static>
        ${QT_LINK_TARGETS}
        )
