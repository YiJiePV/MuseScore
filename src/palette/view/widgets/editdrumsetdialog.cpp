/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "editdrumsetdialog.h"

#include "engraving/infrastructure/io/xml.h"

#include "libmscore/factory.h"
#include "libmscore/utils.h"
#include "libmscore/chord.h"
#include "libmscore/score.h"
#include "libmscore/note.h"
#include "libmscore/stem.h"
#include "libmscore/scorefont.h"
#include "libmscore/sym.h"

#include "engraving/infrastructure/draw/geometry.h"

#include "framework/global/smuflranges.h"

#include <QMessageBox>

using namespace mu::framework;
using namespace mu::notation;
using namespace mu::engraving;

static const QString EDIT_DRUMSET_DIALOG_NAME("EditDrumsetDialog");

namespace Ms {
enum Column : char {
    PITCH, NOTE, SHORTCUT, NAME
};

class EditDrumsetTreeWidgetItem : public QTreeWidgetItem
{
public:
    EditDrumsetTreeWidgetItem(QTreeWidget* parent)
        : QTreeWidgetItem(parent) {}

    bool operator<(const QTreeWidgetItem& other) const override
    {
        if (treeWidget()->sortColumn() == Column::PITCH) {
            return data(Column::PITCH, Qt::UserRole).toInt() < other.data(Column::PITCH, Qt::UserRole).toInt();
        }

        return QTreeWidgetItem::operator<(other);
    }
};

//---------------------------------------------------------
//   noteHeadNames
//   "Sol" and "Alt. Brevis" omitted,
//   as not being useful for drums
//---------------------------------------------------------

NoteHead::Group noteHeadNames[] = {
    NoteHead::Group::HEAD_NORMAL,
    NoteHead::Group::HEAD_CROSS,
    NoteHead::Group::HEAD_PLUS,
    NoteHead::Group::HEAD_XCIRCLE,
    NoteHead::Group::HEAD_WITHX,
    NoteHead::Group::HEAD_TRIANGLE_UP,
    NoteHead::Group::HEAD_TRIANGLE_DOWN,
    NoteHead::Group::HEAD_SLASH,
    NoteHead::Group::HEAD_SLASHED1,
    NoteHead::Group::HEAD_SLASHED2,
    NoteHead::Group::HEAD_DIAMOND,
    NoteHead::Group::HEAD_DIAMOND_OLD,
    NoteHead::Group::HEAD_CIRCLED,
    NoteHead::Group::HEAD_CIRCLED_LARGE,
    NoteHead::Group::HEAD_LARGE_ARROW,
    NoteHead::Group::HEAD_DO,
    NoteHead::Group::HEAD_RE,
    NoteHead::Group::HEAD_MI,
    NoteHead::Group::HEAD_FA,
    NoteHead::Group::HEAD_LA,
    NoteHead::Group::HEAD_TI,
    NoteHead::Group::HEAD_CUSTOM
};

//---------------------------------------------------------
//   EditDrumsetDialog
//---------------------------------------------------------

struct SymbolIcon {
    SymId id;
    QIcon icon;
    SymbolIcon(SymId i, QIcon j)
        : id(i), icon(j)
    {}

    static SymbolIcon generateIcon(const SymId& id, double w, double h, double defaultScale)
    {
        QIcon icon;
        QPixmap image(w, h);
        image.fill(Qt::transparent);
        mu::draw::Painter painter(&image, "generateicon");
        const mu::RectF& bbox = ScoreFont::fallbackFont()->bbox(id, 1);
        const qreal actualSymbolScale = std::min(w / bbox.width(), h / bbox.height());
        qreal mag = std::min(defaultScale, actualSymbolScale);
        const qreal& xStShift = (w - mag * bbox.width()) / 2 - mag * bbox.left();
        const qreal& yStShift = (h - mag * bbox.height()) / 2 - mag * bbox.top();
        const mu::PointF& stPtPos = mu::PointF(xStShift, yStShift);
        ScoreFont::fallbackFont()->draw(id, &painter, mag, stPtPos);
        icon.addPixmap(image);
        return SymbolIcon(id, icon);
    }
};

EditDrumsetDialog::EditDrumsetDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(EDIT_DRUMSET_DIALOG_NAME);

    m_notation = globalContext()->currentNotation();
    if (!m_notation) {
        return;
    }

    const INotationInteractionPtr interaction = m_notation->interaction();
    INotationInteraction::HitElementContext context = interaction->hitElementContext();
    const Measure* measure = toMeasure(context.element);

    if (measure && context.staff) {
        Ms::Instrument* instrument = context.staff->part()->instrument(measure->tick());
        m_instrumentKey.instrumentId = instrument->id();
        m_instrumentKey.partId = context.staff->part()->id();
        m_instrumentKey.tick = measure->tick();
        m_editedDrumset = *instrument->drumset();
    } else {
        NoteInputState state = m_notation->interaction()->noteInput()->state();
        const Staff* staff = m_notation->elements()->msScore()->staff(track2staff(state.currentTrack));
        m_instrumentKey.instrumentId = staff ? staff->part()->instrumentId() : QString();
        m_instrumentKey.partId = staff ? staff->part()->id() : ID();
        m_editedDrumset = state.drumset ? *state.drumset : Drumset();
    }

    setupUi(this);
    setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);

    drumNote->setGridSize(70, 80);
    drumNote->setDrawGrid(false);
    drumNote->setReadOnly(true);

    updatePitchesList();

    for (auto g : noteHeadNames) {
        noteHead->addItem(NoteHead::group2userName(g), int(g));
    }

    connect(pitchList, &QTreeWidget::currentItemChanged, this, &EditDrumsetDialog::itemChanged);
    connect(buttonBox, &QDialogButtonBox::clicked, this, &EditDrumsetDialog::bboxClicked);
    connect(name, &QLineEdit::textChanged, this, &EditDrumsetDialog::nameChanged);
    connect(noteHead, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EditDrumsetDialog::valueChanged);
    connect(staffLine, QOverload<int>::of(&QSpinBox::valueChanged), this, &EditDrumsetDialog::valueChanged);
    connect(voice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EditDrumsetDialog::valueChanged);
    connect(stemDirection, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EditDrumsetDialog::valueChanged);
    connect(shortcut, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EditDrumsetDialog::shortcutChanged);
    connect(loadButton, &QPushButton::clicked, this, &EditDrumsetDialog::load);
    connect(saveButton, &QPushButton::clicked, this, &EditDrumsetDialog::save);
    pitchList->setColumnWidth(0, 40);
    pitchList->setColumnWidth(1, 60);
    pitchList->setColumnWidth(2, 30);

    QStringList validNoteheadRanges
        = { "Noteheads", "Round and square noteheads", "Slash noteheads", "Shape note noteheads", "Shape note noteheads supplement" };
    QSet<QString> excludeSym = { "noteheadParenthesisLeft", "noteheadParenthesisRight", "noteheadParenthesis", "noteheadNull" };
    QStringList primaryNoteheads = {
        "noteheadXOrnate",
        "noteheadXBlack",
        "noteheadXHalf",
        "noteheadXWhole",
        "noteheadXDoubleWhole",
        "noteheadSlashedBlack1",
        "noteheadSlashedHalf1",
        "noteheadSlashedWhole1",
        "noteheadSlashedDoubleWhole1",
        "noteheadSlashedBlack2",
        "noteheadSlashedHalf2",
        "noteheadSlashedWhole2",
        "noteheadSlashedDoubleWhole2",
        "noteheadSquareBlack",
        "noteheadMoonBlack",
        "noteheadTriangleUpRightBlack",
        "noteheadTriangleDownBlack",
        "noteheadTriangleUpBlack",
        "noteheadTriangleLeftBlack",
        "noteheadTriangleRoundDownBlack",
        "noteheadDiamondBlack",
        "noteheadDiamondHalf",
        "noteheadDiamondWhole",
        "noteheadDiamondDoubleWhole",
        "noteheadRoundWhiteWithDot",
        "noteheadVoidWithX",
        "noteheadHalfWithX",
        "noteheadWholeWithX",
        "noteheadDoubleWholeWithX",
        "noteheadLargeArrowUpBlack",
        "noteheadLargeArrowUpHalf",
        "noteheadLargeArrowUpWhole",
        "noteheadLargeArrowUpDoubleWhole"
    };

    int w = quarterCmb->iconSize().width() * qApp->devicePixelRatio();
    int h = quarterCmb->iconSize().height() * qApp->devicePixelRatio();
    //default scale is 0.3, will use smaller scale for large noteheads symbols
    const qreal defaultScale = 0.3 * qApp->devicePixelRatio();

    QList<SymbolIcon> resNoteheads;
    for (auto symName : primaryNoteheads) {
        SymId id = Sym::name2id(symName);
        resNoteheads.append(SymbolIcon::generateIcon(id, w, h, defaultScale));
    }

    for (QString range : validNoteheadRanges) {
        for (auto symName : (*mu::smuflRanges())[range]) {
            SymId id = Sym::name2id(symName);
            if (!excludeSym.contains(symName) && !primaryNoteheads.contains(symName)) {
                resNoteheads.append(SymbolIcon::generateIcon(id, w, h, defaultScale));
            }
        }
    }

    QComboBox* combos[] = { wholeCmb, halfCmb, quarterCmb, doubleWholeCmb };
    for (QComboBox* combo : combos) {
        for (auto si : resNoteheads) {
            SymId id = si.id;
            QIcon icon = si.icon;
            combo->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            combo->addItem(icon, Sym::id2userName(id), Sym::id2name(id));
        }
    }
    wholeCmb->setCurrentIndex(quarterCmb->findData(Sym::id2name(SymId::noteheadWhole)));
    halfCmb->setCurrentIndex(quarterCmb->findData(Sym::id2name(SymId::noteheadHalf)));
    quarterCmb->setCurrentIndex(quarterCmb->findData(Sym::id2name(SymId::noteheadBlack)));
    doubleWholeCmb->setCurrentIndex(quarterCmb->findData(Sym::id2name(SymId::noteheadDoubleWhole)));

    connect(customGbox, &QGroupBox::toggled, this, &EditDrumsetDialog::customGboxToggled);
    connect(quarterCmb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EditDrumsetDialog::customQuarterChanged);

    Q_ASSERT(pitchList->topLevelItemCount() > 0);
    pitchList->setCurrentItem(pitchList->topLevelItem(0));
    pitchList->setFocus();
}

EditDrumsetDialog::EditDrumsetDialog(const EditDrumsetDialog& other)
    : QDialog(other.parentWidget())
{
}

int EditDrumsetDialog::static_metaTypeId()
{
    return QMetaType::type(EDIT_DRUMSET_DIALOG_NAME.toStdString().c_str());
}

//---------------------------------------------------------
//   customGboxToggled
//---------------------------------------------------------

void EditDrumsetDialog::customGboxToggled(bool checked)
{
    noteHead->setEnabled(!checked);
    if (checked) {
        noteHead->setCurrentIndex(noteHead->findData(int(NoteHead::Group::HEAD_CUSTOM)));
    } else {
        noteHead->setCurrentIndex(noteHead->findData(int(NoteHead::Group::HEAD_NORMAL)));
    }
}

//---------------------------------------------------------
//   updatePitchesList
//---------------------------------------------------------

void EditDrumsetDialog::updatePitchesList()
{
    pitchList->clear();
    for (int i = 0; i < 128; ++i) {
        QTreeWidgetItem* item = new EditDrumsetTreeWidgetItem(pitchList);
        item->setText(Column::PITCH, QString("%1").arg(i));
        item->setText(Column::NOTE, pitch2string(i));
        if (m_editedDrumset.shortcut(i) == 0) {
            item->setText(Column::SHORTCUT, "");
        } else {
            QString s(QChar(m_editedDrumset.shortcut(i)));
            item->setText(Column::SHORTCUT, s);
        }
        item->setText(Column::NAME, mu::qtrc("drumset", m_editedDrumset.name(i).toUtf8().constData()));
        item->setData(Column::PITCH, Qt::UserRole, i);
    }
    pitchList->sortItems(3, Qt::SortOrder::DescendingOrder);
}

//---------------------------------------------------------
//   refreshPitchesList
//---------------------------------------------------------
void EditDrumsetDialog::refreshPitchesList()
{
    for (int i = 0; i < pitchList->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = pitchList->topLevelItem(i);
        int pitch = item->data(0, Qt::UserRole).toInt();
        if (m_editedDrumset.shortcut(pitch) == 0) {
            item->setText(Column::SHORTCUT, "");
        } else {
            QString s(QChar(m_editedDrumset.shortcut(pitch)));
            item->setText(Column::SHORTCUT, s);
        }
        item->setText(Column::NAME, mu::qtrc("drumset", m_editedDrumset.name(pitch).toUtf8().constData()));
        item->setData(0, Qt::UserRole, pitch);
    }
}

void EditDrumsetDialog::setEnabledPitchControls(bool enable)
{
    customGbox->setEnabled(enable);
    noteHead->setEnabled(enable);
    voice->setEnabled(enable);
    shortcut->setEnabled(enable);
    staffLine->setEnabled(enable);
    stemDirection->setEnabled(enable);
    drumNote->setEnabled(enable);
    label_2->setEnabled(enable);
    label_3->setEnabled(enable);
    label_4->setEnabled(enable);
    label_5->setEnabled(enable);
    label_6->setEnabled(enable);
}

//---------------------------------------------------------
//   nameChanged
//---------------------------------------------------------

void EditDrumsetDialog::nameChanged(const QString& n)
{
    QTreeWidgetItem* item = pitchList->currentItem();
    if (item) {
        item->setText(Column::NAME, n);
        int pitch = item->data(Column::PITCH, Qt::UserRole).toInt();
        if (!n.isEmpty()) {
            if (!m_editedDrumset.isValid(pitch)) {
                noteHead->setCurrentIndex(0);
            }
        } else {
            m_editedDrumset.drum(pitch).name.clear();
        }
    }
    setEnabledPitchControls(!n.isEmpty());
}

//---------------------------------------------------------
//   shortcutChanged
//---------------------------------------------------------

void EditDrumsetDialog::shortcutChanged()
{
    QTreeWidgetItem* item = pitchList->currentItem();
    if (!item) {
        return;
    }

    int pitch = item->data(Column::PITCH, Qt::UserRole).toInt();
    int sc;
    if (shortcut->currentIndex() == 7) {
        sc = 0;
    } else {
        sc = "ABCDEFG"[shortcut->currentIndex()];
    }

    if (QString(QChar(m_editedDrumset.drum(pitch).shortcut)) != shortcut->currentText()) {
        //
        // remove conflicting shortcuts
        //
        for (int i = 0; i < DRUM_INSTRUMENTS; ++i) {
            if (i == pitch) {
                continue;
            }
            if (m_editedDrumset.drum(i).shortcut == sc) {
                m_editedDrumset.drum(i).shortcut = 0;
            }
        }
        m_editedDrumset.drum(pitch).shortcut = sc;
        if (shortcut->currentIndex() == 7) {
            item->setText(Column::SHORTCUT, "");
        } else {
            item->setText(Column::SHORTCUT, shortcut->currentText());
        }
    }
    refreshPitchesList();
}

//---------------------------------------------------------
//   bboxClicked
//---------------------------------------------------------
void EditDrumsetDialog::bboxClicked(QAbstractButton* button)
{
    switch (buttonBox->buttonRole(button)) {
    case QDialogButtonBox::ApplyRole:
    case QDialogButtonBox::AcceptRole:
        apply();
        break;
    default:
        break;
    }
}

//---------------------------------------------------------
//   apply
//---------------------------------------------------------
void EditDrumsetDialog::apply()
{
    valueChanged();    //save last changes in name
}

//---------------------------------------------------------
//   fillCustomNoteheadsDataFromComboboxes
//---------------------------------------------------------
void EditDrumsetDialog::fillCustomNoteheadsDataFromComboboxes(int pitch)
{
    m_editedDrumset.drum(pitch).notehead = NoteHead::Group::HEAD_CUSTOM;
    m_editedDrumset.drum(pitch).noteheads[int(NoteHead::Type::HEAD_WHOLE)] = Sym::name2id(wholeCmb->currentData().toString());
    m_editedDrumset.drum(pitch).noteheads[int(NoteHead::Type::HEAD_QUARTER)] = Sym::name2id(quarterCmb->currentData().toString());
    m_editedDrumset.drum(pitch).noteheads[int(NoteHead::Type::HEAD_HALF)] = Sym::name2id(halfCmb->currentData().toString());
    m_editedDrumset.drum(pitch).noteheads[int(NoteHead::Type::HEAD_BREVIS)] = Sym::name2id(doubleWholeCmb->currentData().toString());
}

void EditDrumsetDialog::fillNoteheadsComboboxes(bool customGroup, int pitch)
{
    if (customGroup) {
        wholeCmb->setCurrentIndex(quarterCmb->findData(Sym::id2name(m_editedDrumset.noteHeads(pitch, NoteHead::Type::HEAD_WHOLE))));
        halfCmb->setCurrentIndex(quarterCmb->findData(Sym::id2name(m_editedDrumset.noteHeads(pitch, NoteHead::Type::HEAD_HALF))));
        quarterCmb->setCurrentIndex(quarterCmb->findData(Sym::id2name(m_editedDrumset.noteHeads(pitch, NoteHead::Type::HEAD_QUARTER))));
        doubleWholeCmb->setCurrentIndex(quarterCmb->findData(Sym::id2name(m_editedDrumset.noteHeads(pitch, NoteHead::Type::HEAD_BREVIS))));
    } else {
        const auto group = m_editedDrumset.drum(pitch).notehead;
        if (group == NoteHead::Group::HEAD_INVALID) {
            return;
        }

        wholeCmb->setCurrentIndex(quarterCmb->findData(Sym::id2name(Note::noteHead(0, group, NoteHead::Type::HEAD_WHOLE))));
        halfCmb->setCurrentIndex(quarterCmb->findData(Sym::id2name(Note::noteHead(0, group, NoteHead::Type::HEAD_HALF))));
        quarterCmb->setCurrentIndex(quarterCmb->findData(Sym::id2name(Note::noteHead(0, group, NoteHead::Type::HEAD_QUARTER))));
        doubleWholeCmb->setCurrentIndex(quarterCmb->findData(Sym::id2name(Note::noteHead(0, group, NoteHead::Type::HEAD_BREVIS))));
    }
}

//---------------------------------------------------------
//   itemChanged
//---------------------------------------------------------
void EditDrumsetDialog::itemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
    if (previous) {
        int pitch = previous->data(0, Qt::UserRole).toInt();
        m_editedDrumset.drum(pitch).name          = name->text();
        if (customGbox->isChecked()) {
            fillCustomNoteheadsDataFromComboboxes(pitch);
        } else {
            const QVariant currData = noteHead->currentData();
            if (currData.isValid()) {
                m_editedDrumset.drum(pitch).notehead = NoteHead::Group(currData.toInt());
            }
        }

        m_editedDrumset.drum(pitch).line          = staffLine->value();
        m_editedDrumset.drum(pitch).voice         = voice->currentIndex();
        if (shortcut->currentIndex() == 7) {
            m_editedDrumset.drum(pitch).shortcut = 0;
        } else {
            m_editedDrumset.drum(pitch).shortcut = "ABCDEFG"[shortcut->currentIndex()];
        }
        m_editedDrumset.drum(pitch).stemDirection = Direction(stemDirection->currentIndex());
        previous->setText(Column::NAME, mu::qtrc("drumset", m_editedDrumset.name(pitch).toUtf8().constData()));
    }
    if (current == 0) {
        return;
    }

    staffLine->blockSignals(true);
    voice->blockSignals(true);
    stemDirection->blockSignals(true);
    noteHead->blockSignals(true);

    int pitch = current->data(0, Qt::UserRole).toInt();
    name->setText(mu::qtrc("drumset", m_editedDrumset.name(pitch).toUtf8().constData()));
    staffLine->setValue(m_editedDrumset.line(pitch));
    voice->setCurrentIndex(m_editedDrumset.voice(pitch));
    stemDirection->setCurrentIndex(int(m_editedDrumset.stemDirection(pitch)));

    NoteHead::Group nh = m_editedDrumset.noteHead(pitch);
    bool isCustomGroup = (nh == NoteHead::Group::HEAD_CUSTOM);
    if (m_editedDrumset.isValid(pitch)) {
        setCustomNoteheadsGUIEnabled(isCustomGroup);
    }
    noteHead->setCurrentIndex(noteHead->findData(int(nh)));
    fillNoteheadsComboboxes(isCustomGroup, pitch);

    if (m_editedDrumset.shortcut(pitch) == 0) {
        shortcut->setCurrentIndex(7);
    } else {
        shortcut->setCurrentIndex(m_editedDrumset.shortcut(pitch) - 'A');
    }

    staffLine->blockSignals(false);
    voice->blockSignals(false);
    stemDirection->blockSignals(false);
    noteHead->blockSignals(false);

    updateExample();
}

//---------------------------------------------------------
//   setCustomNoteheadsGUIEnabled
//---------------------------------------------------------
void EditDrumsetDialog::setCustomNoteheadsGUIEnabled(bool enabled)
{
    customGbox->setChecked(enabled);
    noteHead->setEnabled(!enabled);
    if (enabled) {
        noteHead->setCurrentIndex(noteHead->findData(int(NoteHead::Group::HEAD_CUSTOM)));
    }
}

//---------------------------------------------------------
//   valueChanged
//---------------------------------------------------------
void EditDrumsetDialog::valueChanged()
{
    if (!pitchList->currentItem()) {
        return;
    }
    int pitch = pitchList->currentItem()->data(Column::PITCH, Qt::UserRole).toInt();
    m_editedDrumset.drum(pitch).name          = name->text();
    if (customGbox->isChecked() || noteHead->currentIndex() == noteHead->findData(int(NoteHead::Group::HEAD_CUSTOM))) {
        fillCustomNoteheadsDataFromComboboxes(pitch);
        setCustomNoteheadsGUIEnabled(true);
    } else {
        m_editedDrumset.drum(pitch).notehead = NoteHead::Group(noteHead->currentData().toInt());
        fillNoteheadsComboboxes(false, pitch);
        setCustomNoteheadsGUIEnabled(false);
    }

    m_editedDrumset.drum(pitch).line          = staffLine->value();
    m_editedDrumset.drum(pitch).voice         = voice->currentIndex();
    m_editedDrumset.drum(pitch).stemDirection = Direction(stemDirection->currentIndex());
    if (QString(QChar(m_editedDrumset.drum(pitch).shortcut)) != shortcut->currentText()) {
        if (shortcut->currentText().isEmpty()) {
            m_editedDrumset.drum(pitch).shortcut = 0;
        } else {
            m_editedDrumset.drum(pitch).shortcut = shortcut->currentText().at(0).toLatin1();
        }
    }
    updateExample();

    m_notation->parts()->replaceDrumset(m_instrumentKey, m_editedDrumset);
}

//---------------------------------------------------------
//   updateExample
//---------------------------------------------------------
void EditDrumsetDialog::updateExample()
{
    drumNote->clear();
    int pitch = pitchList->currentItem()->data(0, Qt::UserRole).toInt();
    if (!m_editedDrumset.isValid(pitch)) {
        return;
    }
    int line      = m_editedDrumset.line(pitch);
    NoteHead::Group nh = m_editedDrumset.noteHead(pitch);
    int v         = m_editedDrumset.voice(pitch);
    Direction dir = m_editedDrumset.stemDirection(pitch);
    bool up = (Direction::UP == dir) || (Direction::AUTO == dir && line > 4);
    std::shared_ptr<Chord> chord = std::make_shared<Chord>(gpaletteScore->dummy()->segment());
    chord->setDurationType(TDuration::DurationType::V_QUARTER);
    chord->setStemDirection(dir);
    chord->setTrack(v);
    chord->setUp(up);
    Note* note = Factory::createNote(chord.get());
    note->setParent(chord.get());
    note->setTrack(v);
    note->setPitch(pitch);
    note->setTpcFromPitch();
    note->setLine(line);
    note->setPos(0.0, gpaletteScore->spatium() * .5 * line);
    note->setHeadType(NoteHead::Type::HEAD_QUARTER);
    note->setHeadGroup(nh);
    note->setCachedNoteheadSym(Sym::name2id(quarterCmb->currentData().toString()));
    chord->add(note);
    Stem* stem = Factory::createStem(chord.get());
    stem->setLen((up ? -3.0 : 3.0) * gpaletteScore->spatium());
    chord->add(stem);
    drumNote->appendElement(chord, mu::qtrc("drumset", m_editedDrumset.name(pitch).toUtf8().constData()));
}

//---------------------------------------------------------
//   load
//---------------------------------------------------------

void EditDrumsetDialog::load()
{
    QString filter = mu::qtrc("palette", "MuseScore Drumset File") + " (*.drm)";
    mu::io::path dir = notationConfiguration()->userStylesPath();
    mu::io::path fname = interactive()->selectOpeningFile(mu::qtrc("palette", "Load Drumset"), dir, filter);

    if (fname.empty()) {
        return;
    }

    QFile fp(fname.toQString());
    if (!fp.open(QIODevice::ReadOnly)) {
        return;
    }

    XmlReader e(&fp);
    m_editedDrumset.clear();
    while (e.readNextStartElement()) {
        if (e.name() == "museScore") {
            if (e.attribute("version") != MSC_VERSION) {
                auto result = interactive()->warning(
                    mu::trc("palette", "Drumset file too old"),
                    mu::trc("palette", "MuseScore may not be able to load this drumset file."), {
                        IInteractive::Button::Cancel,
                        IInteractive::Button::Ignore
                    }, IInteractive::Button::Cancel);

                if (result.standartButton() != IInteractive::Button::Ignore) { // covers Cancel and Esc
                    return;
                }
            }
            while (e.readNextStartElement()) {
                if (e.name() == "Drum") {
                    m_editedDrumset.load(e);
                } else {
                    e.unknown();
                }
            }
        }
    }
    fp.close();
    updatePitchesList();
}

//---------------------------------------------------------
//   save
//---------------------------------------------------------

void EditDrumsetDialog::save()
{
    QString filter = mu::qtrc("palette", "MuseScore Drumset File") + " (*.drm)";
    mu::io::path dir = notationConfiguration()->userStylesPath();
    mu::io::path fname = interactive()->selectOpeningFile(mu::qtrc("palette", "Save Drumset"), dir, filter);

    if (fname.empty()) {
        return;
    }

    QFile f(fname.toQString());
    if (!f.open(QIODevice::WriteOnly)) {
        QString s = mu::qtrc("palette", "Open File\n%1\nfailed: %2").arg(f.fileName()).arg(strerror(errno));
        interactive()->error(mu::trc("palette", "Open File"), s.toStdString());
        return;
    }
    valueChanged();    //save last changes in name
    XmlWriter xml(0, &f);
    xml.header();
    xml.stag("museScore version=\"" MSC_VERSION "\"");
    m_editedDrumset.save(xml);
    xml.etag();
    if (f.error() != QFile::NoError) {
        QString s = mu::qtrc("palette", "Write File failed: %1").arg(f.errorString());
        interactive()->error(mu::trc("palette", "Write Drumset"), s.toStdString());
    }
}

//---------------------------------------------------------
//   customQuarterChanged
//---------------------------------------------------------
void EditDrumsetDialog::customQuarterChanged(int)
{
    updateExample();
}
}
