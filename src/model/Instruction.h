/*======================================================================================================================
 * model/Instruction.h is part of Brewken, and is copyright the following authors 2009-2024:
 *   • Brian Rower <brian.rower@gmail.com>
 *   • Jeff Bailey <skydvr38@verizon.net>
 *   • Matt Young <mfsy@yahoo.com>
 *   • Mik Firestone <mikfire@gmail.com>
 *   • Philip Greggory Lee <rocketman768@gmail.com>
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
 =====================================================================================================================*/
#ifndef MODEL_INSTRUCTION_H
#define MODEL_INSTRUCTION_H
#pragma once

#include <QDomNode>
#include <QString>
#include <QVector>

#include "model/NamedEntity.h"
#include "model/SteppedBase.h"

//======================================================================================================================
//========================================== Start of property name constants ==========================================
// See comment in model/NamedEntity.h
#define AddPropertyName(property) namespace PropertyNames::Instruction { BtStringConst const property{#property}; }
AddPropertyName(completed )
AddPropertyName(directions)
AddPropertyName(hasTimer  )
AddPropertyName(interval  )
AddPropertyName(timerValue)
#undef AddPropertyName
//=========================================== End of property name constants ===========================================
//======================================================================================================================

// Forward declarations;
class Recipe;

/*!
 * \class Instruction
 *
 * \brief Model class for an instruction record in the database.
 *
 *        NOTE that \c Instruction is not part of the official BeerXML or BeerJSON standards.  We add it in to our
 *             BeerXML files, because we can, but TBD whether this is possible with BeerJSON.
 *
 *        NB: We do not inherit from \c OwnedByRecipe, because doing so would duplicate part of what we get from
 *            \c SteppedBase.
 */
class Instruction : public NamedEntity,
                    public SteppedBase<Instruction, Recipe> {
   Q_OBJECT

   STEPPED_COMMON_DECL(Instruction, Recipe)
   // See model/SteppedBase.h for info, getters and setters for these properties
   Q_PROPERTY(int ownerId      READ ownerId      WRITE setOwnerId   )
   Q_PROPERTY(int stepNumber   READ stepNumber   WRITE setStepNumber)

public:
   /**
    * \brief See comment in model/NamedEntity.h
    */
   static QString localisedName();

   /**
    * \brief Mapping of names to types for the Qt properties of this class.  See \c NamedEntity::typeLookup for more
    *        info.
    */
   static TypeLookup const typeLookup;
   TYPE_LOOKUP_GETTER

   Instruction(QString name = "");
   Instruction(NamedParameterBundle const & namedParameterBundle);
   Instruction(Instruction const & other);

   virtual ~Instruction();

   //=================================================== PROPERTIES ====================================================
   Q_PROPERTY(QString        directions        READ directions         WRITE setDirections)
   Q_PROPERTY(bool           hasTimer          READ hasTimer           WRITE setHasTimer  )
   Q_PROPERTY(QString        timerValue        READ timerValue         WRITE setTimerValue)
   Q_PROPERTY(bool           completed         READ completed          WRITE setCompleted )
   Q_PROPERTY(double         interval          READ interval           WRITE setInterval  )
   Q_PROPERTY(QList<QString> reagents          READ reagents           STORED false       )

   //============================================ "GETTER" MEMBER FUNCTIONS ============================================
   QString directions();
   bool    hasTimer();
   QString timerValue();
   bool    completed();
   //! This is a non-stored temporary in-memory set.
   QList<QString> reagents();
   double interval();

   //============================================ "SETTER" MEMBER FUNCTIONS ============================================
   void setDirections(const QString& dir);
   void setHasTimer  (bool has);
   void setTimerValue(const QString& timerVal);
   void setCompleted (bool comp);
   void setInterval  (double interval);
   void addReagent   (const QString& reagent);

///   virtual std::shared_ptr<Recipe> owningRecipe() const override;

signals:

protected:
   virtual bool isEqualTo(NamedEntity const & other) const override;

private:
   QString m_directions;
   bool    m_hasTimer;
   QString m_timerValue;
   bool    m_completed;
   double  m_interval;

   QList<QString> m_reagents;
};

BT_DECLARE_METATYPES(Instruction)

//! \brief Compares Instruction pointers by Instruction::instructionNumber().
bool operator<(Instruction & lhs, Instruction & rhs);

#endif
