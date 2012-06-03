#include "resurrectpenaltydialog.h"


const int ResurrectPenaltyDialog::kLevelPenalty = 10;
const double ResurrectPenaltyDialog::kSkillPenalty = 0.025;
const double ResurrectPenaltyDialog::kStatPenalty = 0.04;

ResurrectPenaltyDialog::ResurrectPenaltyDialog(QWidget *parent) : QDialog(parent)
{
    ui.setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowModality(Qt::WindowModal);

    resize(sizeHint());
    setFixedSize(size());

    ui.buttonBox->setFocus();
}

ResurrectPenaltyDialog::ResurrectionPenalty ResurrectPenaltyDialog::resurrectionPenalty() const
{
    if (ui.levelsRadioButton->isChecked())
        return Levels;
    else if (ui.skillsRadioButton->isChecked())
        return Skills;
    else if (ui.statsRadioButton->isChecked())
        return Stats;
    else
        return Nothing;
}
