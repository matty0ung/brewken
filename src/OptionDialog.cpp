/**
 * OptionDialog.cpp is part of Brewken, and is copyright the following authors 2009-2021:
 *   • Brian Rower <brian.rower@gmail.com>
 *   • Daniel Pettersson <pettson81@gmail.com>
 *   • Greg Meess <Daedalus12@gmail.com>
 *   • Idar Lund <idarlund@gmail.com>
 *   • Mark de Wever <koraq@xs4all.nl>
 *   • Mattias Måhl <mattias@kejsarsten.com>
 *   • Matt Young <mfsy@yahoo.com>
 *   • Maxime Lavigne <duguigne@gmail.com>
 *   • Mik Firestone <mikfire@gmail.com>
 *   • Philip Greggory Lee <rocketman768@gmail.com>
 *   • Rob Taylor <robtaylor@floopily.org>
 *   • Théophane Martin <theophane.m@gmail.com>
 *
 * Brewken is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * Brewken is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
#include <QMessageBox>
#include <QFileDialog>

#include "OptionDialog.h"
#include "Brewken.h"
#include "BtLineEdit.h"
#include "unitSystems/UnitSystems.h"
#include "unitSystems/USWeightUnitSystem.h"
#include "unitSystems/SIWeightUnitSystem.h"
#include "unitSystems/ImperialVolumeUnitSystem.h"
#include "unitSystems/USVolumeUnitSystem.h"
#include "unitSystems/SIVolumeUnitSystem.h"
#include "unitSystems/FahrenheitTempUnitSystem.h"
#include "unitSystems/EbcColorUnitSystem.h"
#include "unitSystems/SrmColorUnitSystem.h"
#include "unitSystems/PlatoDensityUnitSystem.h"
#include "unitSystems/SgDensityUnitSystem.h"
#include "unitSystems/DiastaticPowerUnitSystem.h"
#include "unitSystems/CelsiusTempUnitSystem.h"
#include "database/Database.h"
#include "MainWindow.h"

OptionDialog::OptionDialog(QWidget* parent)
{
   int i;

   // I need a lot of control over what is displayed on the DbConfig dialog.
   // Maybe designer can do it? No idea. So I did this hybrid model, and I
   // think it will end up biting my ...
   // anyway. It isn't pretty
   setupUi(this);
   createPostgresElements();
   createSQLiteElements();

   if( parent != nullptr ) {
      setWindowIcon(parent->windowIcon());
   }

   ndxToLangCode <<
      "ca" <<
      "cs" <<
      "da" <<
      "de" <<
      "el" <<
      "en" <<
      "es" <<
      "et" <<
      "eu" <<
      "fr" <<
      "gl" <<
      "hu" <<
      "it" <<
      "lv" <<
      "nb" <<
      "nl" <<
      "pl" <<
      "pt" <<
      "ru" <<
      "sr" <<
      "sv" <<
      "tr" <<
      "zh";

   // Do this just to have model indices to set icons.
   comboBox_lang->addItems(ndxToLangCode);
   // MUST correspond to ndxToLangCode.
   langIcons <<
      /*ca*/ QIcon(":images/flagCatalonia.svg") <<
      /*cs*/ QIcon(":images/flagCzech.svg") <<
      /*da*/ QIcon(":images/flagDenmark.svg") <<
      /*de*/ QIcon(":images/flagGermany.svg") <<
      /*el*/ QIcon(":images/flagGreece.svg") <<
      /*en*/ QIcon(":images/flagUK.svg") <<
      /*es*/ QIcon(":images/flagSpain.svg") <<
      /*et*/ QIcon() <<
      /*eu*/ QIcon() <<
      /*fr*/ QIcon(":images/flagFrance.svg") <<
      /*gl*/ QIcon() <<
      /*hu*/ QIcon() <<
      /*it*/ QIcon(":images/flagItaly.svg") <<
      /*lv*/ QIcon() <<
      /*nb*/ QIcon(":images/flagNorway.svg") <<
      /*nl*/ QIcon(":images/flagNetherlands.svg") <<
      /*pl*/ QIcon(":images/flagPoland.svg") <<
      /*pt*/ QIcon(":images/flagBrazil.svg") <<
      /*ru*/ QIcon(":images/flagRussia.svg") <<
      /*sr*/ QIcon() <<
      /*sv*/ QIcon(":images/flagSweden.svg") <<
      /*tr*/ QIcon() <<
      /*zh*/ QIcon(":images/flagChina.svg");
   // Set icons.
   for( i = 0; i < langIcons.size(); ++i )
      comboBox_lang->setItemIcon(i, langIcons[i]);

   // Call this here to set up translatable strings.
   retranslate();

   // Populate combo boxes on the "Units" tab
   weightComboBox->addItem(tr("SI units"), QVariant(SI));
   weightComboBox->addItem(tr("US traditional units"), QVariant(USCustomary));
   weightComboBox->addItem(tr("British imperial units"), QVariant(Imperial));

   temperatureComboBox->addItem(tr("Celsius"), QVariant(Celsius));
   temperatureComboBox->addItem(tr("Fahrenheit"), QVariant(Fahrenheit));

   volumeComboBox->addItem(tr("SI units"), QVariant(SI));
   volumeComboBox->addItem(tr("US traditional units"), QVariant(USCustomary));
   volumeComboBox->addItem(tr("British imperial units"), QVariant(Imperial));

   gravityComboBox->addItem(tr("20C/20C Specific Gravity"), QVariant(Brewken::SG));
   gravityComboBox->addItem(tr("Plato/Brix/Balling"), QVariant(Brewken::PLATO));

   dateComboBox->addItem(tr("mm-dd-YYYY"), QVariant(Unit::displayUS));
   dateComboBox->addItem(tr("dd-mm-YYYY"), QVariant(Unit::displayImp));
   dateComboBox->addItem(tr("YYYY-mm-dd"), QVariant(Unit::displaySI));

   colorComboBox->addItem(tr("SRM"), QVariant(Brewken::SRM));
   colorComboBox->addItem(tr("EBC"), QVariant(Brewken::EBC));

   diastaticPowerComboBox->addItem(tr("Lintner"), QVariant(Brewken::LINTNER));
   diastaticPowerComboBox->addItem(tr("WK"), QVariant(Brewken::WK));

   // Populate combo boxes on the "Formulas" tab
   ibuFormulaComboBox->addItem(tr("Tinseth's approximation"), QVariant(Brewken::TINSETH));
   ibuFormulaComboBox->addItem(tr("Rager's approximation"), QVariant(Brewken::RAGER));
   ibuFormulaComboBox->addItem(tr("Noonan's approximation"), QVariant(Brewken::NOONAN));

   colorFormulaComboBox->addItem(tr("Mosher's approximation"), QVariant(Brewken::MOSHER));
   colorFormulaComboBox->addItem(tr("Daniel's approximation"), QVariant(Brewken::DANIEL));
   colorFormulaComboBox->addItem(tr("Morey's approximation"), QVariant(Brewken::MOREY));

   connect( buttonBox, &QDialogButtonBox::accepted, this, &OptionDialog::saveAndClose );
   connect( buttonBox, &QDialogButtonBox::rejected, this, &OptionDialog::cancel );

   //Populate options on the "Logging" tab
   for (auto ii : Logging::levelDetails) {
      loggingLevelComboBox->addItem(ii.description, QVariant(ii.level));
   }
   loggingLevelComboBox->setCurrentIndex(Logging::logLevel);
   checkBox_LogFileLocationUseDefault->setChecked(Logging::logUseConfigDir);
   lineEdit_LogFileLocation->setText(Logging::getDirectory().absolutePath());
   setFileLocationState(Logging::logUseConfigDir);

   // database panel stuff
   comboBox_engine->addItem( tr("SQLite (default)"), QVariant(Brewken::SQLITE));
   comboBox_engine->addItem( tr("PostgreSQL"), QVariant(Brewken::PGSQL));
   connect( comboBox_engine, SIGNAL( currentIndexChanged(int) ), this, SLOT( setEngine(int) ) );
   connect( pushButton_testConnection, &QAbstractButton::clicked, this, &OptionDialog::testConnection);

   // figure out which database we have
   int idx = comboBox_engine->findData(Brewken::option("dbType", Brewken::SQLITE).toInt());
   setDbDialog(static_cast<Brewken::DBTypes>(idx));

   // Set the signals
   connect( checkBox_savePassword, &QAbstractButton::clicked, this, &OptionDialog::savePassword);
   connect( checkBox_LogFileLocationUseDefault, &QAbstractButton::clicked, this, &OptionDialog::setFileLocationState);

   connect( btStringEdit_hostname, &BtLineEdit::textModified, this, &OptionDialog::testRequired);
   connect( btStringEdit_portnum, &BtLineEdit::textModified, this, &OptionDialog::testRequired);
   connect( btStringEdit_schema, &BtLineEdit::textModified, this, &OptionDialog::testRequired);
   connect( btStringEdit_dbname, &BtLineEdit::textModified, this, &OptionDialog::testRequired);
   connect( btStringEdit_username, &BtLineEdit::textModified, this, &OptionDialog::testRequired);
   connect( btStringEdit_password, &BtLineEdit::textModified, this, &OptionDialog::testRequired);

   connect( pushButton_browseDataDir, &QAbstractButton::clicked, this, &OptionDialog::setDataDir );
   connect( pushButton_browseBackupDir, &QAbstractButton::clicked, this, &OptionDialog::setBackupDir );
   connect( pushButton_resetToDefault, &QAbstractButton::clicked, this, &OptionDialog::resetToDefault );
   connect( pushButton_LogFileLocationBrowse, &QAbstractButton::clicked, this, &OptionDialog::setLogDir );
   pushButton_testConnection->setEnabled(false);

}

void OptionDialog::retranslate()
{
   // Let the Ui take care of its business
   retranslateUi(this);
   retranslateDbDialog(this);

   // Retranslate the language combobox.
   // NOTE: the indices MUST correspond to ndxToLangCode.
   QStringList langStrings;
   langStrings <<
      /*ca*/ tr("Catalan") <<
      /*cs*/ tr("Czech") <<
      /*da*/ tr("Danish") <<
      /*de*/ tr("German") <<
      /*el*/ tr("Greek") <<
      /*en*/ tr("English") <<
      /*es*/ tr("Spanish") <<
      /*et*/ tr("Estonian") <<
      /*eu*/ tr("Basque") <<
      /*fr*/ tr("French") <<
      /*gl*/ tr("Galician") <<
      /*hu*/ tr("Hungarian") <<
      /*it*/ tr("Italian") <<
      /*lv*/ tr("Latvian") <<
      /*nb*/ tr("Norwegian Bokmål") <<
      /*nl*/ tr("Dutch") <<
      /*pl*/ tr("Polish") <<
      /*pt*/ tr("Portuguese") <<
      /*ru*/ tr("Russian") <<
      /*sr*/ tr("Serbian") <<
      /*sv*/ tr("Swedish") <<
      /*tr*/ tr("Turkish") <<
      /*zh*/ tr("Chinese");
   int i;
   for( i = 0; i < langStrings.size(); ++i )
      comboBox_lang->setItemText(i, langStrings[i]);
}

void OptionDialog::show()
{
   showChanges();
   setVisible(true);
}

void OptionDialog::setDataDir()
{
   QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"), Brewken::getUserDataDir().canonicalPath(), QFileDialog::ShowDirsOnly);
   if( ! dir.isEmpty() )
      btStringEdit_dataDir->setText( dir );
}

void OptionDialog::setBackupDir()
{
   QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"), Brewken::getUserDataDir().canonicalPath(), QFileDialog::ShowDirsOnly);
   if( ! dir.isEmpty() )
      btStringEdit_backupDir->setText( dir );
}

void OptionDialog::setLogDir()
{
   QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"), Brewken::getUserDataDir().canonicalPath(), QFileDialog::ShowDirsOnly);
   if( ! dir.isEmpty() )
      lineEdit_LogFileLocation->setText( dir );
}

void OptionDialog::resetToDefault()
{
   Brewken::DBTypes engine = static_cast<Brewken::DBTypes>(comboBox_engine->currentData().toInt());
   if ( engine == Brewken::PGSQL ) {
      btStringEdit_hostname->setText(QString("localhost"));
      btStringEdit_portnum->setText(QString("5432"));
      btStringEdit_schema->setText(QString("public"));
      btStringEdit_dbname->setText(QString("brewken"));
      btStringEdit_username->setText(QString("brewken"));
      btStringEdit_password->setText(QString(""));
      checkBox_savePassword->setChecked(false);
   }
   else {
      btStringEdit_dataDir->setText( Brewken::getConfigDir().canonicalPath() );
      btStringEdit_backupDir->setText( Brewken::getConfigDir().canonicalPath() );
      spinBox_frequency->setValue(4);
      spinBox_numBackups->setValue(10);
   }
}

void OptionDialog::saveAndClose()
{
   bool okay = false;
   bool saveDbConfig = true;

   // TODO:: FIX THIS UI. I am really not sure what the best approach is here.
   if ( status == OptionDialog::NEEDSTEST || status == OptionDialog::TESTFAILED ) {
      QMessageBox::critical(nullptr,
            tr("Test connection or cancel"),
            tr("Saving the options without testing the connection can cause Brewken to not restart. Your changes have been discarded, which is likely really, really crappy UX. Please open a bug explaining exactly how you got to this message.")
            );
      return;
   }

   if ( status == OptionDialog::TESTPASSED ) {
      // This got unpleasant. There are multiple possible transfer paths.
      // SQLite->Pgsql, Pgsql->Pgsql and Pgsql->SQLite. This will ensure we
      // preserve the information required.
      try {
         QString theQuestion = tr("Would you like Brewken to transfer your data to the new database? NOTE: If you've already loaded the data, say No");
         if ( QMessageBox::Yes == QMessageBox::question(this, tr("Transfer database"), theQuestion) ) {
            Database::instance().convertDatabase(btStringEdit_hostname->text(), btStringEdit_dbname->text(),
                                                 btStringEdit_username->text(), btStringEdit_password->text(),
                                                 btStringEdit_portnum->text().toInt(),
                                                 static_cast<Brewken::DBTypes>(comboBox_engine->currentData().toInt()));
         }
         // Database engine stuff
         int engine = comboBox_engine->currentData().toInt();
         Brewken::setOption("dbType", engine);
         // only write these changes when switching TO pgsql
         if ( engine == Brewken::PGSQL ) {
            Brewken::setOption("dbHostname", btStringEdit_hostname->text());
            Brewken::setOption("dbPortnum", btStringEdit_portnum->text());
            Brewken::setOption("dbSchema", btStringEdit_schema->text());
            Brewken::setOption("dbName", btStringEdit_dbname->text());
            Brewken::setOption("dbUsername", btStringEdit_username->text());
         }
         QMessageBox::information(this, tr("Restart"), tr("Please restart Brewken to connect to the new database"));
      }
      catch (QString e) {
         qCritical() << Q_FUNC_INFO << e;
         saveDbConfig = false;
      }
   }

   if ( saveDbConfig && checkBox_savePassword->checkState() == Qt::Checked ) {
      Brewken::setOption("dbPassword", btStringEdit_password->text());
   }
   else {
      Brewken::removeOption("dbPassword");
   }

   switch (weightComboBox->itemData(weightComboBox->currentIndex()).toInt(&okay))
   {
      case SI:
      default:
         Brewken::weightUnitSystem = SI;
         Brewken::thingToUnitSystem.insert(Unit::Mass, UnitSystems::siWeightUnitSystem());
         break;
      case USCustomary:
         Brewken::weightUnitSystem  = USCustomary;
         Brewken::thingToUnitSystem.insert(Unit::Mass, UnitSystems::usWeightUnitSystem());
         break;
      case Imperial:
         Brewken::weightUnitSystem  = Imperial;
         Brewken::thingToUnitSystem.insert(Unit::Mass, UnitSystems::usWeightUnitSystem());
         break;
   }

   switch (temperatureComboBox->itemData(temperatureComboBox->currentIndex()).toInt(&okay))
   {
      case Celsius:
      default:
         Brewken::tempScale = Celsius;
         Brewken::thingToUnitSystem.insert(Unit::Temp,UnitSystems::celsiusTempUnitSystem());
         break;
      case Fahrenheit:
         Brewken::tempScale = Fahrenheit;
         Brewken::thingToUnitSystem.insert(Unit::Temp,UnitSystems::fahrenheitTempUnitSystem());
         break;
   }

   switch (volumeComboBox->itemData(volumeComboBox->currentIndex()).toInt(&okay))
   {
      case SI:
      default:
         Brewken::volumeUnitSystem = SI;
         Brewken::thingToUnitSystem.insert(Unit::Volume,UnitSystems::siVolumeUnitSystem());
         break;
      case USCustomary:
         Brewken::volumeUnitSystem = USCustomary;
         Brewken::thingToUnitSystem.insert(Unit::Volume,UnitSystems::usVolumeUnitSystem());
         break;
      case Imperial:
         Brewken::volumeUnitSystem = Imperial;
         Brewken::thingToUnitSystem.insert(Unit::Volume,UnitSystems::imperialVolumeUnitSystem());
         break;
   }

   switch (gravityComboBox->itemData(gravityComboBox->currentIndex()).toInt(&okay))
   {
      case Brewken::SG:
      default:
         Brewken::densityUnit = Brewken::SG;
         Brewken::thingToUnitSystem.insert(Unit::Density, UnitSystems::sgDensityUnitSystem());
         break;
      case Brewken::PLATO:
         Brewken::densityUnit = Brewken::PLATO;
         Brewken::thingToUnitSystem.insert(Unit::Density, UnitSystems::platoDensityUnitSystem());
         break;
   }

   switch (dateComboBox->itemData(dateComboBox->currentIndex()).toInt(&okay))
   {
      case Unit::displayUS:
      default:
         Brewken::dateFormat = Unit::displayUS;
         break;
      case Unit::displayImp:
         Brewken::dateFormat = Unit::displayImp;
         break;
      case Unit::displaySI:
         Brewken::dateFormat = Unit::displaySI;
         break;
   }

   switch (colorComboBox->itemData(colorComboBox->currentIndex()).toInt(&okay))
   {
      case Brewken::SRM:
      default:
         Brewken::thingToUnitSystem.insert(Unit::Color,UnitSystems::srmColorUnitSystem());
         Brewken::colorUnit = Brewken::SRM;
         break;
      case Brewken::EBC:
         Brewken::thingToUnitSystem.insert(Unit::Color,UnitSystems::ebcColorUnitSystem());
         Brewken::colorUnit = Brewken::EBC;
         break;
   }

   switch (diastaticPowerComboBox->itemData(diastaticPowerComboBox->currentIndex()).toInt(&okay))
   {
      case Brewken::LINTNER:
      default:
         Brewken::thingToUnitSystem.insert(Unit::DiastaticPower,UnitSystems::lintnerDiastaticPowerUnitSystem());
         Brewken::diastaticPowerUnit = Brewken::LINTNER;
         break;
      case Brewken::WK:
         Brewken::thingToUnitSystem.insert(Unit::DiastaticPower,UnitSystems::wkDiastaticPowerUnitSystem());
         Brewken::diastaticPowerUnit = Brewken::WK;
         break;
   }

   int ndx = ibuFormulaComboBox->itemData(ibuFormulaComboBox->currentIndex()).toInt(&okay);
   Brewken::ibuFormula = static_cast<Brewken::IbuType>(ndx);
   ndx = colorFormulaComboBox->itemData(colorFormulaComboBox->currentIndex()).toInt(&okay);
   Brewken::colorFormula = static_cast<Brewken::ColorType>(ndx);

   // Set the right language.
   Brewken::setLanguage( ndxToLangCode[ comboBox_lang->currentIndex() ] );

   // Check the new userDataDir.
   Brewken::DBTypes dbEngine = static_cast<Brewken::DBTypes>(comboBox_engine->currentData().toInt());
   if ( dbEngine == Brewken::SQLITE ) {
      QString newUserDataDir = btStringEdit_dataDir->text();
      QDir userDirectory(newUserDataDir);

      // I think this is redundant and could be handled as just a simple db
      // transfer using the testPassed loop above.
      if( userDirectory != Brewken::getUserDataDir() )
      {
         // If there are no data files present...
         if( ! QFileInfo(userDirectory, "database.sqlite").exists() )
         {
            // ...tell user we will copy old data files to new location.
            QMessageBox::information(this,
                                    tr("Copy Data"),
                                    tr("There do not seem to be any data files in this directory, so we will copy your old data here.")
                                    );
            Brewken::copyDataFiles(newUserDataDir);
         }

         Brewken::userDataDir.setPath(newUserDataDir);
         Brewken::setOption("user_data_dir", newUserDataDir);
         QMessageBox::information(
            this,
            tr("Restart"),
            tr("Please restart Brewken.")
         );
      }

      Brewken::setOption("maximum", spinBox_numBackups->value(), "backups");
      Brewken::setOption("frequency", spinBox_frequency->value(), "backups");
      Brewken::setOption("directory", btStringEdit_backupDir->text(), "backups");
   }

   Brewken::setOption("mashHopAdjustment", ibuAdjustmentMashHopDoubleSpinBox->value() / 100);
   Brewken::setOption("firstWortHopAdjustment", ibuAdjustmentFirstWortDoubleSpinBox->value() / 100);

   // Saving Logging Options to the Log object
   Logging::logLevel = static_cast<Logging::Level>(loggingLevelComboBox->currentData().toInt());
   Logging::setDirectory(QDir(lineEdit_LogFileLocation->text()));
   Logging::logUseConfigDir = checkBox_LogFileLocationUseDefault->isChecked();
   if ( Logging::logUseConfigDir )
   {
      Logging::setDirectory(Brewken::getConfigDir());
   }
   Brewken::setOption("LoggingLevel", Logging::getStringFromLogLevel(Logging::logLevel));
   Brewken::setOption("LogFilePath", Logging::getDirectory().absolutePath());
   Brewken::setOption("LoggingUseConfigDir", Logging::logUseConfigDir);
   // Make sure the main window updates.
   if( Brewken::mainWindow() )
      Brewken::mainWindow()->showChanges();

   setVisible(false);
}

void OptionDialog::cancel()
{
   setVisible(false);
}

void OptionDialog::showChanges()
{
   // Set the right language
   int ndx = ndxToLangCode.indexOf( Brewken::getCurrentLanguage() );
   if( ndx >= 0 )
      comboBox_lang->setCurrentIndex(ndx);


   weightComboBox->setCurrentIndex(weightComboBox->findData(Brewken::weightUnitSystem));
   temperatureComboBox->setCurrentIndex(temperatureComboBox->findData(Brewken::tempScale));
   volumeComboBox->setCurrentIndex(volumeComboBox->findData(Brewken::volumeUnitSystem));
   gravityComboBox->setCurrentIndex(gravityComboBox->findData(Brewken::densityUnit));
   dateComboBox->setCurrentIndex(dateComboBox->findData(Brewken::dateFormat));
   colorComboBox->setCurrentIndex(colorComboBox->findData(Brewken::colorUnit));
   diastaticPowerComboBox->setCurrentIndex(diastaticPowerComboBox->findData(Brewken::diastaticPowerUnit));

   colorFormulaComboBox->setCurrentIndex(colorFormulaComboBox->findData(Brewken::colorFormula));
   ibuFormulaComboBox->setCurrentIndex(ibuFormulaComboBox->findData(Brewken::ibuFormula));

   // Data directory
   btStringEdit_dataDir->setText(Brewken::getUserDataDir().canonicalPath());

   // Backup stuff
   btStringEdit_backupDir->setText( Brewken::option("directory", Brewken::getUserDataDir().canonicalPath(), "backups").toString() );
   spinBox_numBackups->setValue( Brewken::option("maximum", 10, "backups").toInt() );
   spinBox_frequency->setValue( Brewken::option("frequency", 4, "backups").toInt() );

   // The IBU modifications. These will all be calculated from a 60 min boil. This is gonna get confusing.
   double amt = Brewken::toDouble(Brewken::option("mashHopAdjustment",0).toString(), "OptionDialog::showChanges()");
   ibuAdjustmentMashHopDoubleSpinBox->setValue(amt*100);

   amt = Brewken::toDouble(Brewken::option("firstWortHopAdjustment",1.1).toString(), "OptionDialog::showChanges()");
   ibuAdjustmentFirstWortDoubleSpinBox->setValue(amt*100);

   // Database stuff -- this looks weird, but trust me. We want SQLITE to be
   // the default for this field
   int tmp = Brewken::option("dbType",Brewken::SQLITE).toInt() - 1;
   comboBox_engine->setCurrentIndex(tmp);

   btStringEdit_hostname->setText(Brewken::option("dbHostname","localhost").toString());
   btStringEdit_portnum->setText(Brewken::option("dbPort","5432").toString());
   btStringEdit_schema->setText(Brewken::option("dbSchema","public").toString());
   btStringEdit_dbname->setText(Brewken::option("dbName","brewken").toString());
   btStringEdit_username->setText(Brewken::option("dbUsername","brewken").toString());
   btStringEdit_password->setText(Brewken::option("dbPassword","").toString());
   checkBox_savePassword->setChecked( Brewken::hasOption("dbPassword") );

   status = OptionDialog::NOCHANGE;
   changeColors();
}

void OptionDialog::postgresVisible(bool canSee)
{
   label_hostname->setVisible(canSee);
   btStringEdit_hostname->setVisible(canSee);
   label_portnum->setVisible(canSee);
   btStringEdit_portnum->setVisible(canSee);
   label_schema->setVisible(canSee);
   btStringEdit_schema->setVisible(canSee);
   label_dbName->setVisible(canSee);
   btStringEdit_dbname->setVisible(canSee);
   label_username->setVisible(canSee);
   btStringEdit_username->setVisible(canSee);
   label_password->setVisible(canSee);
   btStringEdit_password->setVisible(canSee);
   checkBox_savePassword->setVisible(canSee);
   label_password->setVisible(canSee);
}

void OptionDialog::sqliteVisible(bool canSee)
{
   label_dataDir->setVisible(canSee);
   btStringEdit_dataDir->setVisible(canSee);

   pushButton_browseDataDir->setVisible(canSee);
   label_backupDir->setVisible(canSee);
   btStringEdit_backupDir->setVisible(canSee);
   pushButton_browseBackupDir->setVisible(canSee);

   label_numBackups->setVisible(canSee);
   spinBox_numBackups->setVisible(canSee);

   label_frequency->setVisible(canSee);
   spinBox_frequency->setVisible(canSee);
}

void OptionDialog::setDbDialog(Brewken::DBTypes db)
{
   groupBox_dbConfig->setVisible(false);

   clearLayout();
   if ( db == Brewken::PGSQL ) {
      postgresVisible(true);
      sqliteVisible(false);

      gridLayout->addWidget(label_hostname,0,0);
      gridLayout->addWidget(btStringEdit_hostname,0,1,1,2);

      gridLayout->addWidget(label_portnum,0,3);
      gridLayout->addWidget(btStringEdit_portnum,0,4);

      gridLayout->addWidget(label_schema,1,0);
      gridLayout->addWidget(btStringEdit_schema,1,1);

      gridLayout->addWidget(label_dbName,2,0);
      gridLayout->addWidget(btStringEdit_dbname,2,1);

      gridLayout->addWidget(label_username,3,0);
      gridLayout->addWidget(btStringEdit_username,3,1);

      gridLayout->addWidget(label_password,4,0);
      gridLayout->addWidget(btStringEdit_password,4,1);

      gridLayout->addWidget(checkBox_savePassword, 4, 4);

   }
   else {
      postgresVisible(false);
      sqliteVisible(true);

      gridLayout->addWidget(label_dataDir,0,0);
      gridLayout->addWidget(btStringEdit_dataDir,0,1,1,2);
      gridLayout->addWidget(pushButton_browseDataDir,0,3);

      gridLayout->addWidget(label_backupDir,1,0);
      gridLayout->addWidget(btStringEdit_backupDir,1,1,1,2);
      gridLayout->addWidget(pushButton_browseBackupDir,1,3);

      gridLayout->addWidget(label_numBackups,3,0);
      gridLayout->addWidget(spinBox_numBackups,3,1);

      gridLayout->addWidget(label_frequency,4,0);
      gridLayout->addWidget(spinBox_frequency,4,1);
   }
   groupBox_dbConfig->setVisible(true);
}

void OptionDialog::clearLayout()
{
   QLayoutItem *child;

   while ( (child = gridLayout->takeAt(0)) != nullptr ) {
      gridLayout->removeItem(child);
   }
}

void OptionDialog::createPostgresElements()
{

   label_hostname = new QLabel(groupBox_dbConfig);
   label_hostname->setObjectName(QStringLiteral("label_hostname"));

   btStringEdit_hostname = new BtStringEdit(groupBox_dbConfig);
   btStringEdit_hostname->setObjectName(QStringLiteral("btStringEdit_hostname"));

   label_portnum = new QLabel(groupBox_dbConfig);
   label_portnum->setObjectName(QStringLiteral("label_portnum"));

   btStringEdit_portnum = new BtStringEdit(groupBox_dbConfig);
   btStringEdit_portnum->setObjectName(QStringLiteral("btStringEdit_portnum"));

   label_schema = new QLabel(groupBox_dbConfig);
   label_schema->setObjectName(QStringLiteral("label_schema"));

   btStringEdit_schema = new BtStringEdit(groupBox_dbConfig);
   btStringEdit_schema->setObjectName(QStringLiteral("btStringEdit_schema"));

   label_dbName = new QLabel(groupBox_dbConfig);
   label_dbName->setObjectName(QStringLiteral("label_dbName"));

   btStringEdit_dbname = new BtStringEdit(groupBox_dbConfig);
   btStringEdit_dbname->setObjectName(QStringLiteral("btStringEdit_dbname"));

   label_username = new QLabel(groupBox_dbConfig);
   label_username->setObjectName(QStringLiteral("label_username"));

   btStringEdit_username = new BtStringEdit(groupBox_dbConfig);
   btStringEdit_username->setObjectName(QStringLiteral("btStringEdit_username"));

   btStringEdit_password = new BtStringEdit(groupBox_dbConfig);
   btStringEdit_password->setObjectName(QStringLiteral("btStringEdit_password"));
   btStringEdit_password->setEchoMode(QLineEdit::Password);

   checkBox_savePassword = new QCheckBox(groupBox_dbConfig);
   checkBox_savePassword->setObjectName(QStringLiteral("checkBox_savePassword"));

   label_password = new QLabel(groupBox_dbConfig);
   label_password->setObjectName(QStringLiteral("label_password"));

   postgresVisible(false);
}

void OptionDialog::createSQLiteElements()
{

   // Oy vey. Set up the data directory dialog and buttons
   label_dataDir = new QLabel(groupBox_dbConfig);
   label_dataDir->setObjectName(QStringLiteral("label_dataDir"));

   btStringEdit_dataDir = new BtStringEdit(groupBox_dbConfig);
   btStringEdit_dataDir->setObjectName(QStringLiteral("btStringEdit_dataDir"));

   pushButton_browseDataDir = new QPushButton(groupBox_dbConfig);
   pushButton_browseDataDir->setObjectName(QStringLiteral("button_browseDataDir"));

   // Set up the backup directory dialog and buttons
   label_backupDir = new QLabel(groupBox_dbConfig);
   label_backupDir->setObjectName(QStringLiteral("label_backupDir"));

   btStringEdit_backupDir = new BtStringEdit(groupBox_dbConfig);
   btStringEdit_backupDir->setObjectName(QStringLiteral("btStringEdit_backupDir"));

   pushButton_browseBackupDir = new QPushButton(groupBox_dbConfig);
   pushButton_browseBackupDir->setObjectName(QStringLiteral("button_browseBackupDir"));

   // Set up the two spin boxes
   label_numBackups = new QLabel(groupBox_dbConfig);
   label_numBackups->setObjectName(QStringLiteral("label_numBackups"));

   spinBox_numBackups = new QSpinBox(groupBox_dbConfig);
   spinBox_numBackups->setObjectName(QStringLiteral("spinBox_numBackups"));
   spinBox_numBackups->setMinimum(-1);
   spinBox_numBackups->setMaximum(9999);

   label_frequency = new QLabel(groupBox_dbConfig);
   label_frequency->setObjectName(QStringLiteral("label_frequency"));

   spinBox_frequency = new QSpinBox(groupBox_dbConfig);
   spinBox_frequency->setObjectName(QStringLiteral("spinBox_frequency"));
   // I couldn't make any semantic difference between 0 and 1. So we start at
   // 1
   spinBox_frequency->setMinimum(1);
   spinBox_frequency->setMaximum(10);

   sqliteVisible(false);

}

void OptionDialog::retranslateDbDialog(QDialog *optionsDialog)
{
   //PostgreSQL stuff
   label_hostname->setText(QApplication::translate("optionsDialog", "Hostname", nullptr));
   label_portnum->setText(QApplication::translate("optionsDialog", "Port", nullptr));
   label_schema->setText(QApplication::translate("optionsDialog", "Schema", nullptr));
   label_dbName->setText(QApplication::translate("optionsDialog", "Database", nullptr));
   label_username->setText(QApplication::translate("optionsDialog", "Username", nullptr));
   label_password->setText(QApplication::translate("optionsDialog", "Password", nullptr));
   checkBox_savePassword->setText(QApplication::translate("optionsDialog", "Save password", nullptr));

   // SQLite things
   label_dataDir->setText(QApplication::translate("optionsDialog", "Data Directory", nullptr));
   pushButton_browseDataDir->setText(QApplication::translate("optionsDialog", "Browse", nullptr));
   label_backupDir->setText(QApplication::translate("optionsDialog", "Backup Directory", nullptr));
   pushButton_browseBackupDir->setText(QApplication::translate("optionsDialog", "Browse", nullptr));
   label_numBackups->setText(QApplication::translate("optionsDialog", "Number of Backups", nullptr));
   label_frequency->setText(QApplication::translate("optionsDialog", "Frequency of Backups", nullptr));

   // set up the tooltips if we are using them
#ifndef QT_NO_TOOLTIP
   btStringEdit_hostname->setToolTip(QApplication::translate("optionsDialog", "PostgresSQL's host name or IP address", nullptr));
   btStringEdit_portnum->setToolTip(QApplication::translate("optionsDialog", "Port the PostgreSQL is listening on", nullptr));
   btStringEdit_schema->setToolTip(QApplication::translate("optionsDialog", "The schema containing the database", nullptr));
   btStringEdit_username->setToolTip(QApplication::translate("optionsDialog", "User with create/delete table access", nullptr));
   btStringEdit_password->setToolTip(QApplication::translate("optionsDialog", "Password for the user", nullptr));
   btStringEdit_dbname->setToolTip(QApplication::translate("optionsDialog", "The name of the database", nullptr));
   label_dataDir->setToolTip(QApplication::translate("optionsDialog", "Where your database file is",nullptr));
   label_backupDir->setToolTip(QApplication::translate("optionsDialog", "Where to save your backups",nullptr));
   label_numBackups->setToolTip(QApplication::translate("optionsDialog", "Number of backups to keep: -1 means never remove, 0 means never backup", nullptr));
   label_frequency->setToolTip(QApplication::translate("optionsDialog", "How frequently a backup is made: 1 means always backup", nullptr));
#endif
}

void OptionDialog::changeEvent(QEvent* e)
{
   switch( e->type() )
   {
      case QEvent::LanguageChange:
         retranslate();
         e->accept();
         break;
      default:
         QDialog::changeEvent(e);
         break;
   }
}

void OptionDialog::setEngine(int selected)
{

   QVariant data = comboBox_engine->currentData();
   Brewken::DBTypes newEngine = static_cast<Brewken::DBTypes>(data.toInt());

   setDbDialog(newEngine);
   testRequired();

}

void OptionDialog::testConnection()
{
   bool success;
   QString hostname, schema, database, username, password;
   int port;

   Brewken::DBTypes newType = static_cast<Brewken::DBTypes>(comboBox_engine->currentData().toInt());
   // Do nothing if nothing is required.
   if ( status == OptionDialog::NOCHANGE || status == OptionDialog::TESTPASSED)
   {
      return;
   }

   switch( newType )
   {
      case Brewken::PGSQL:
         hostname = btStringEdit_hostname->text();
         schema   = btStringEdit_schema->text();
         database = btStringEdit_dbname->text();
         username = btStringEdit_username->text();
         password = btStringEdit_password->text();
         port     = (btStringEdit_portnum->text()).toInt();

         success = Database::verifyDbConnection(newType,hostname,port,schema,database,username,password);
         break;
      default:
         hostname = QString("%1/%2").arg(btStringEdit_dataDir->text()).arg("database.sqlite");
         success = Database::verifyDbConnection(newType,hostname);
   }

   if ( success )
   {
      QMessageBox::information(nullptr,
                           QObject::tr("Connection Test"),
                           QString(QObject::tr("Connection to database was successful"))
                           );
      status = OptionDialog::TESTPASSED;
   }
   else
   {
      // Database::testConnection already popped the dialog
      status = OptionDialog::TESTFAILED;
   }
   changeColors();
}

void OptionDialog::testRequired()
{
   status = OptionDialog::NEEDSTEST;
   changeColors();
}

void OptionDialog::changeColors()
{
   // Yellow when the test is needed
   // Red when the test failed
   // Green when the test passed
   // Black otherwise.

   switch(status)
   {
      case OptionDialog::NEEDSTEST:
         buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
         pushButton_testConnection->setEnabled(true);
         pushButton_testConnection->setStyleSheet("color:rgb(240,225,25)");
         break;
      case OptionDialog::TESTFAILED:
         buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
         pushButton_testConnection->setStyleSheet("color:red");
         break;
      case OptionDialog::TESTPASSED:
         pushButton_testConnection->setStyleSheet("color:green");
         buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
         pushButton_testConnection->setEnabled(false);
         break;
      case OptionDialog::NOCHANGE:
         pushButton_testConnection->setStyleSheet("color:grey");
         buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
         pushButton_testConnection->setEnabled(false);
         break;
   }
}

void OptionDialog::savePassword(bool state)
{
   if ( state ) {
      QMessageBox::warning(nullptr, QObject::tr("Plaintext"),
                              QObject::tr("Passwords are saved in plaintext. We make no effort to hide, obscure or otherwise protect the password. By enabling this option, you take full responsibility for any potential problems."));
   }
}

void OptionDialog::setFileLocationState(bool state)
{
   lineEdit_LogFileLocation->setEnabled( ! state );
   pushButton_LogFileLocationBrowse->setEnabled( ! state );
}
