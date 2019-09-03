//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2019 Werner Schweer and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//=============================================================================

import QtQuick 2.8
import QtQuick.Controls 2.1

import MuseScore.Utils 3.3

Item {
    id: paletteHeader

    property bool expanded: false
    property string text: ""
    property bool hidePaletteElementVisible

    signal toggleExpandRequested()
    signal hideSelectedElementsRequested()
    signal hidePaletteRequested()
    signal editPalettePropertiesRequested()

    implicitHeight: paletteExpandArrow.height
    StyledToolButton {
        id: paletteExpandArrow
        z: 1000
        width: height
        visible: paletteHeader.text.length // TODO: make a separate palette placeholder component
        text: paletteHeader.expanded ? qsTr("Collapse") : qsTr("Expand")

        padding: 0

        contentItem: StyledIcon {
            source: paletteHeader.expanded ? "icons/arrow_drop_down.png" : "icons/arrow_right_black.png"
        }

        onClicked: paletteHeader.toggleExpandRequested()
    }
    Text {
        height: parent.height
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHLeft
        anchors {
            left: paletteExpandArrow.right; leftMargin: 4;
            right: deleteButton.visible ? deleteButton.left : paletteHeaderMenuButton.left
        }
        text: paletteHeader.text
        font: globalStyle.font
        color: globalStyle.text
        elide: Text.ElideRight
    }
//     StyledToolButton {
//         z: 1000
//         height: parent.height
//         anchors { left: paletteExpandArrow.right }
//         text: paletteHeader.text
//     }

    StyledToolButton {
        id: deleteButton
        z: 1000
        height: parent.height
        width: height
        anchors.right: paletteHeaderMenuButton.left
//         icon.name: "delete" // can't use icon until Qt 5.10... https://doc.qt.io/qt-5/qtquickcontrols2-icons.html
//         icon.source: "icons/delete.png"
        text: qsTr("Remove element")
        visible: paletteHeader.hidePaletteElementVisible

        ToolTip.visible: hovered
        ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
        ToolTip.text: text

        padding: 4

        contentItem: StyledIcon {
            source: "icons/delete.png"
        }

        onClicked: hideSelectedElementsRequested()
    }

    StyledToolButton {
        id: paletteHeaderMenuButton
        z: 1000
        height: parent.height
        anchors.right: parent.right

        padding: 4

        contentItem: StyledIcon {
            source: "icons/more.png"
        }

        Accessible.name: qsTr("Palette menu")

        onClicked: {
            paletteHeaderMenu.x = paletteHeaderMenuButton.x + paletteHeaderMenuButton.width - paletteHeaderMenu.width;
            paletteHeaderMenu.y = paletteHeaderMenuButton.y;
//             paletteHeaderMenu.y = paletteHeaderMenuButton.y + paletteHeaderMenuButton.height;
            paletteHeaderMenu.open();
        }
    }

    MouseArea {
        id: rightClickArea
        anchors.fill: parent
        acceptedButtons: Qt.RightButton

        onClicked: {
            if (paletteHeaderMenu.popup) // Menu.popup() is available since Qt 5.10 only
                paletteHeaderMenu.popup();
            else {
                paletteHeaderMenu.x = mouseX;
                paletteHeaderMenu.y = mouseY;
                paletteHeaderMenu.open();
            }
        }
    }

    Menu {
        id: paletteHeaderMenu
        MenuItem {
            text: qsTr("Hide")
            onTriggered: paletteHeader.hidePaletteRequested()
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Palette Properties")
            onTriggered: paletteHeader.editPalettePropertiesRequested()
        }
    }
}
