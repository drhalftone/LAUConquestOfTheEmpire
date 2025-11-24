#ifndef MAPEDITORWINDOW_H
#define MAPEDITORWINDOW_H

#include <QMainWindow>
#include <QSpinBox>
#include <QPushButton>
#include <QGroupBox>
#include <QLabel>

class EditableGridWidget;

class MapEditorWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MapEditorWindow(QWidget *parent = nullptr);
    ~MapEditorWindow() = default;

private slots:
    void onGenerateMap();
    void onSaveMap();
    void onLoadMap();
    void onClearMap();
    void onNumPlayersChanged(int value);
    void onFlagsChanged();

private:
    void setupUI();
    void createMapSettingsGroup();
    void createGameSettingsGroup();
    void createMapEditorGroup();
    void createButtonsGroup();

    // Map settings
    QGroupBox *m_mapSettingsGroup;
    QSpinBox *m_rowsSpinBox;
    QSpinBox *m_colsSpinBox;
    QSpinBox *m_numPlayersSpinBox;
    QPushButton *m_generateButton;

    // Game settings
    QGroupBox *m_gameSettingsGroup;
    QSpinBox *m_startingGeneralsSpinBox;
    QSpinBox *m_startingInfantrySpinBox;
    QSpinBox *m_startingCavalrySpinBox;
    QSpinBox *m_startingCatapultsSpinBox;
    QSpinBox *m_startingMoneySpinBox;

    // Map editor
    QGroupBox *m_mapEditorGroup;
    EditableGridWidget *m_gridWidget;
    QLabel *m_instructionsLabel;
    QLabel *m_flagStatusLabel;

    // Bottom buttons
    QPushButton *m_saveButton;
    QPushButton *m_loadButton;
    QPushButton *m_clearButton;
};

#endif // MAPEDITORWINDOW_H
