/*======================================================================================================================
 * model/Salt.h is part of Brewken, and is copyright the following authors 2009-2024:
 *   • Jeff Bailey <skydvr38@verizon.net>
 *   • Mattias Måhl <mattias@kejsarsten.com>
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
#ifndef MODEL_SALT_H
#define MODEL_SALT_H
#pragma once

#include <QString>
#include <QSqlRecord>
#include <QSqlRecord>

#include "model/Ingredient.h"
#include "model/IngredientBase.h"
#include "model/IngredientAmount.h"
#include "utils/EnumStringMapping.h"

class InventorySalt;
class RecipeAdjustmentSalt;

//======================================================================================================================
//========================================== Start of property name constants ==========================================
// See comment in model/NamedEntity.h
#define AddPropertyName(property) namespace PropertyNames::Salt { BtStringConst const property{#property}; }
AddPropertyName(isAcid         )
AddPropertyName(percentAcid    )
AddPropertyName(type           )
#undef AddPropertyName
//=========================================== End of property name constants ===========================================
//======================================================================================================================


/*!
 * \class Salt
 *
 * \brief Model for salt records in the database.
 *
 *        NOTE that, unlike most of the other \c NamedEntity classes, \c Salt is not included anywhere in either BeerXML
 *        or BeerJSON.
 */
class Salt : public Ingredient, public IngredientBase<Salt> {
   Q_OBJECT

   INGREDIENT_BASE_DECL(Salt)

public:
   /**
    * \brief See comment in model/NamedEntity.h
    */
   static QString localisedName();

   enum class Type {
      CaCl2         , // Calcium chloride
      CaCO3         , // Calcium carbonate
      CaSO4         , // Calcium sulfate.    See also Gypsum = CaSO4·2H2O
      MgSO4         , // Magnesium sulfate.  See also Epsom salt = MgSO4·7H2O
      NaCl          , // Sodium chloride  aka  "regular" salt
      NaHCO3        , // Sodium bicarbonate
      LacticAcid    , // Lactic acid = CH3CH(OH)COOH
      H3PO4         , // Phosphoric acid
      AcidulatedMalt,
   };
   // This allows us to store the above enum class in a QVariant
   Q_ENUM(Type)

   /*!
    * \brief Mapping between \c Salt::Type and string values suitable for serialisation in DB
    *
    *        This can also be used to obtain the number of values of \c Type, albeit at run-time rather than
    *        compile-time.  (One day, C++ will have reflection and we won't need to do things this way.)
    */
   static EnumStringMapping const typeStringMapping;

   /*!
    * \brief Localised names of \c Water::Type values suitable for displaying to the end user
    */
   static EnumStringMapping const typeDisplayNames;

   /**
    * \brief This is where we centrally define how \c Salt objects can be measured.  See also suggestedMeasure() below
    */
   static constexpr auto validMeasures  = Measurement::ChoiceOfPhysicalQuantity::Mass_Volume;
   static constexpr auto defaultMeasure = Measurement::PhysicalQuantity::Mass;

   //
   // These aliases make it easier to template a number of functions that are essentially the same for all subclasses of
   // Ingredient.
   //
   using InventoryClass      = InventorySalt;
   using RecipeAdditionClass = RecipeAdjustmentSalt;

   /**
    * \brief Mapping of names to types for the Qt properties of this class.  See \c NamedEntity::typeLookup for more
    *        info.
    */
   static TypeLookup const typeLookup;
   TYPE_LOOKUP_GETTER

   Salt(QString name = "");
   Salt(NamedParameterBundle const & namedParameterBundle);
   Salt(Salt const & other);

   virtual ~Salt();

   // On a base or target profile, bicarbonate and alkalinity cannot both be used. I'm gonna have fun figuring that out
   //! \brief What kind of salt this is
   Q_PROPERTY(Type      type           READ type           WRITE setType            )
   /**
    * \brief What percent is acid - valid only for lactic acid, H3PO4 and acid malts
    */
   Q_PROPERTY(std::optional<double>    percentAcid    READ percentAcid    WRITE setPercentAcid     )
   //! \brief Is this an acid or salt?  Deduced from \c type
   Q_PROPERTY(bool      isAcid         READ isAcid)
   Q_PROPERTY(Measurement::PhysicalQuantity suggestedMeasure READ suggestedMeasure)

   Salt::Type            type       () const;
   std::optional<double> percentAcid() const;
   bool                  isAcid     () const;
   Measurement::PhysicalQuantity suggestedMeasure() const;

   void setType       (Salt::Type val);
   void setPercentAcid(std::optional<double> val);
///   void setIsAcid     (bool       val);

   /**
    * \return Mass concentration (in parts per million) of Calcium (Ca) for one gram of this salt in one liter of water
    */
   double massConcPpm_Ca_perGramPerLiter  () const;

   /**
    * \return Mass concentration (in parts per million) of Chloride (Cl⁻) for one gram of this salt in one liter of water
    */
   double massConcPpm_Cl_perGramPerLiter  () const;

   /**
    * \return Mass concentration (in parts per million) of Carbonate (CO₃) for one gram of this salt in one liter of water
    */
   double massConcPpm_CO3_perGramPerLiter () const;

   /**
    * \return Mass concentration (in parts per million) of Bicarbonate (HCO₃) for one gram of this salt in one liter of water
    */
   double massConcPpm_HCO3_perGramPerLiter() const;

   /**
    * \return Mass concentration (in parts per million) of Magnesium (Mg) for one gram of this salt in one liter of water
    */
   double massConcPpm_Mg_perGramPerLiter  () const;

   /**
    * \return Mass concentration (in parts per million) of Sodium (Na⁺) for one gram of this salt in one liter of water
    */
   double massConcPpm_Na_perGramPerLiter  () const;

   /**
    * \return Mass concentration (in parts per million) of Sulfate (SO₄) for one gram of this salt in one liter of water
    */
   double massConcPpm_SO4_perGramPerLiter () const;

signals:

protected:
   virtual bool isEqualTo(NamedEntity const & other) const;
   virtual ObjectStore & getObjectStoreTypedInstance() const;

private:
   Salt::Type            m_type;
   std::optional<double> m_percent_acid;
};

BT_DECLARE_METATYPES(Salt)

#endif
