/**
 * database/DbRecords.cpp is part of Brewken, and is copyright the following authors 2021:
 *   • Matt Young <mfsy@yahoo.com>
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
#include "database/DbRecords.h"

#include <cstring>

#include <QDebug>
#include <QHash>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

#include "database/Database.h"

// Private implementation details that don't need access to class member variables
namespace {
   /**
    * Given a (QVariant-wrapped) string value pulled out of the DB for an enum, look up and return its internal
    * numerical enum equivalent
    */
   int stringToEnum(DbRecords::FieldSimpleDefn const & fieldDefn, QVariant const & valueFromDb) {
      // It's a coding error if we called this function for a non-enum field
      Q_ASSERT(fieldDefn.fieldType == DbRecords::Enum);
      Q_ASSERT(fieldDefn.enumMapping != nullptr);

      QString stringValue = valueFromDb.toString();
      auto match = std::find_if(
         fieldDefn.enumMapping->begin(),
         fieldDefn.enumMapping->end(),
         [stringValue](DbRecords::EnumAndItsDbString const & ii){return stringValue == ii.string;}
      );

      // If we didn't find a match, its either a coding error or someone messed with the DB data
      if (match == fieldDefn.enumMapping->end()) {
         qCritical() <<
            Q_FUNC_INFO << "Could not decode " << stringValue << " to enum when mapping column " <<
            fieldDefn.columnName << " to property " << fieldDefn.propertyName << " so using 0";
         return 0;
      }
      return match->native;
   }

   /**
    * Given a (QVariant-wrapped) int value of a native enum, look up and return the corresponding string we use to
    * store it in the DB
    */
   QString enumToString(DbRecords::FieldSimpleDefn const & fieldDefn, QVariant const & propertyValue) {
      // It's a coding error if we called this function for a non-enum field
      Q_ASSERT(fieldDefn.fieldType == DbRecords::Enum);
      Q_ASSERT(fieldDefn.enumMapping != nullptr);

      int nativeValue = propertyValue.toInt();
      auto match = std::find_if(
         fieldDefn.enumMapping->begin(),
         fieldDefn.enumMapping->end(),
         [nativeValue](DbRecords::EnumAndItsDbString const & ii){return nativeValue == ii.native;}
      );

      // It's a coding error if we couldn't find a match
      Q_ASSERT(match != fieldDefn.enumMapping->end());

      return match->string;
   }

   /**
    * RAII wrapper for transaction(), commit(), rollback() member functions of QSqlDatabase
    */
   class DbTransaction {
   public:
      DbTransaction(QSqlDatabase & databaseConnection) : databaseConnection(databaseConnection), committed(false) {
         bool succeeded = this->databaseConnection.transaction();
         qDebug() << Q_FUNC_INFO << "Database transaction begin: " << (succeeded ? "succeeded" : "failed");
         return;
      }
      ~DbTransaction() {
         qDebug() << Q_FUNC_INFO;
         if (!committed) {
            bool succeeded = this->databaseConnection.rollback();
            qDebug() << Q_FUNC_INFO << "Database transaction rollback: " << (succeeded ? "succeeded" : "failed");
         }
         return;
      }
      bool commit() {
         this->committed = databaseConnection.commit();
         qDebug() << Q_FUNC_INFO << "Database transaction commit: " << (this->committed ? "succeeded" : "failed");
         return this->committed;
      }
   private:
      // This is intended to be a short-lived object, so it's OK to store a reference to a QSqlDatabase object
      QSqlDatabase & databaseConnection;
      bool committed;
   };

   //
   // Insert data from an object property to a junction table
   //
   // We may be inserting more than one row.  In theory we COULD combine all the rows into a single insert statement
   // using either QSqlQuery::execBatch() or directly constructing one of the common (but technically non-standard)
   // syntaxes, eg the following works on a lot of databases (including PostgreSQL and newer versions of SQLite) for
   // up to 1000 rows):
   //    INSERT INTO table (columnA, columnB, ..., columnN)
   //         VALUES       (r1_valA, r1_valB, ..., r1_valN),
   //                      (r2_valA, r2_valB, ..., r2_valN),
   //                      ...,
   //                      (rm_valA, rm_valB, ..., rm_valN);
   // However, we DON"T do this.  The variable binding is more complicated/error-prone than when just doing
   // individual inserts.  (Even with QSqlQuery::execBatch(), we'd have to loop to construct the lists of bind
   // parameters.)  And there's likely no noticeable performance benefit given that we're typically inserting only
   // a handful of rows at a time (eg all the Hops in a Recipe).
   //
   // So instead, we just do individual inserts.  Note that orderByColumn column is only used if specified, and
   // that, if it is, we assume it's an integer type and that we create the values ourselves.
   //
   bool insertIntoFieldManyToManyDefn(DbRecords::FieldManyToManyDefn const & fieldManyToManyDefn,
                                QObject const & object,
                                QVariant const & primaryKey,
                                QSqlDatabase & databaseConnection) {
      qDebug() <<
         Q_FUNC_INFO << "Writing property " << fieldManyToManyDefn.propertyName << " into junction table " <<
         fieldManyToManyDefn.tableName;

      // Construct the query
      QString queryString{"INSERT INTO "};
      QTextStream queryStringAsStream{&queryString};
      queryStringAsStream << fieldManyToManyDefn.tableName << " (" <<
         fieldManyToManyDefn.thisPrimaryKeyColumn << ", " <<
         fieldManyToManyDefn.otherPrimaryKeyColumn;
      if (fieldManyToManyDefn.orderByColumn != "") {
         queryStringAsStream << ", " << fieldManyToManyDefn.orderByColumn;
      }
      QString const thisPrimaryKeyBindName  = QString{":%1"}.arg(fieldManyToManyDefn.thisPrimaryKeyColumn);
      QString const otherPrimaryKeyBindName = QString{":%1"}.arg(fieldManyToManyDefn.otherPrimaryKeyColumn);
      QString const orderByBindName         = QString{":%1"}.arg(fieldManyToManyDefn.orderByColumn);
      queryStringAsStream << ") VALUES (" << thisPrimaryKeyBindName << ", " << otherPrimaryKeyBindName;
      if (fieldManyToManyDefn.orderByColumn != "") {
         queryStringAsStream << ", " << orderByBindName;
      }
      queryStringAsStream << ");";

      QSqlQuery sqlQuery{queryString, databaseConnection};

      // Get the list of data to bind to it
      QVariant bindValues = object.property(fieldManyToManyDefn.propertyName);
      if (fieldManyToManyDefn.assumeMaxOneEntry) {
         // If it's single entry only, just turn it into a one-item list so that the remaining processing is the same
         bindValues = QVariant{QList<QVariant>{bindValues}};
      }

      // Now loop through and bind/run the insert query once for each item in the list
      int itemNumber = 1;
      QList<QVariant> list = bindValues.toList();
      for (auto curValue : list ) {
         sqlQuery.bindValue(thisPrimaryKeyBindName, QVariant{primaryKey});
         sqlQuery.bindValue(otherPrimaryKeyBindName, curValue);
         if (fieldManyToManyDefn.orderByColumn != "") {
            sqlQuery.bindValue(orderByBindName, QVariant{itemNumber});
         }
         qDebug() <<
            Q_FUNC_INFO << itemNumber << ": " << fieldManyToManyDefn.thisPrimaryKeyColumn << " #" << primaryKey <<
            " <-> " << fieldManyToManyDefn.otherPrimaryKeyColumn << " #" << curValue.toInt();

         if (!sqlQuery.exec()) {
            qCritical() <<
               Q_FUNC_INFO << "Error executing database query " << queryString << ": " << sqlQuery.lastError().text();
            return false;
         }
         ++itemNumber;
      }

      return true;
   }

   bool deleteFromFieldManyToManyDefn(DbRecords::FieldManyToManyDefn const & fieldManyToManyDefn,
                                QVariant const & primaryKey,
                                QSqlDatabase & databaseConnection) {

      qDebug() <<
         Q_FUNC_INFO << "Deleting property " << fieldManyToManyDefn.propertyName << " in junction table " <<
         fieldManyToManyDefn.tableName;

      QString const thisPrimaryKeyBindName  = QString{":%1"}.arg(fieldManyToManyDefn.thisPrimaryKeyColumn);

      // Construct the DELETE query
      QString queryString{"DELETE FROM "};
      QTextStream queryStringAsStream{&queryString};
      queryStringAsStream <<
         fieldManyToManyDefn.tableName << " WHERE " << fieldManyToManyDefn.thisPrimaryKeyColumn << " = " <<
         thisPrimaryKeyBindName << ";";

      QSqlQuery sqlQuery{queryString, databaseConnection};

      // Bind the primary key value
      sqlQuery.bindValue(thisPrimaryKeyBindName, primaryKey);

      // Run the query
      if (!sqlQuery.exec()) {
         qCritical() <<
            Q_FUNC_INFO << "Error executing database query " << queryString << ": " << sqlQuery.lastError().text();
         return false;
      }

      return true;
   }

}

// This private implementation class holds all private non-virtual members of DbRecords
class DbRecords::impl {
public:

   /**
    * Constructor
    */
   impl(char const * const tableName,
        FieldSimpleDefns const & fieldSimpleDefns,
        FieldManyToManyDefns const & fieldManyToManyDefns) : tableName{tableName},
                                                           fieldSimpleDefns{fieldSimpleDefns},
                                                           fieldManyToManyDefns{fieldManyToManyDefns},
                                                           allObjects{} {
      return;
   }

   /**
    * Destructor
    */
   ~impl() = default;

   /**
    * \brief Append, to the supplied query string we are constructing, a comma-separated list of all the column names
    *        for the table, in the order of this->fieldSimpleDefns
    *
    * \param queryStringAsStream
    * \param includePrimaryKey  Usually \c true for SELECT and UPDATE, and \c false for INSERT
    * \param prependColons Set to \c true if we are appending bind values
    */
   void appendColumNames(QTextStream & queryStringAsStream, bool includePrimaryKey, bool prependColons) {
      bool skippedPrimaryKey = false;
      bool firstFieldOutput = false;
      for (auto const & fieldDefn: this->fieldSimpleDefns) {
         if (!includePrimaryKey && !skippedPrimaryKey) {
            // By convention the first field is the primary key
            skippedPrimaryKey = true;
         } else {
            if (!firstFieldOutput) {
               firstFieldOutput = true;
            } else {
               queryStringAsStream << ", ";
            }
            if (prependColons) {
               queryStringAsStream << ":";
            }
            queryStringAsStream << fieldDefn.columnName;
         }
      }
      return;
   }

   char const * const tableName;
   FieldSimpleDefns const & fieldSimpleDefns;
   FieldManyToManyDefns const & fieldManyToManyDefns;
   QHash<int, std::shared_ptr<QObject> > allObjects;
};


DbRecords::DbRecords(char const * const tableName,
                     FieldSimpleDefns const & fieldSimpleDefns,
                     FieldManyToManyDefns const & fieldManyToManyDefns) :
   pimpl{ new impl{tableName, fieldSimpleDefns, fieldManyToManyDefns} } {
   return;
}

// See https://herbsutter.com/gotw/_100/ for why we need to explicitly define the destructor here (and not in the
// header file)
DbRecords::~DbRecords() = default;

void DbRecords::createTables() {
   // .:TODO:.
   QString queryString{"CREATE TABLE "};
   QTextStream queryStringAsStream{&queryString};
   queryStringAsStream << this->pimpl->tableName << " (\n";
   bool firstFieldOutput = false;
   for (auto const & fieldDefn: this->pimpl->fieldSimpleDefns) {
      if (!firstFieldOutput) {
         firstFieldOutput = true;
      } else {
         queryStringAsStream << ", \n";
      }
      queryStringAsStream << fieldDefn.columnName << " ";
      // **************************************************************************************************************
      // .:TODO:. In order to finish this, we need a SQL data type mapper, because different DBs have different data
      // types with different names.  This should be hidden from this class, as it shouldn't need to know the specifics
      // of individual databases.
      // **************************************************************************************************************
      switch (fieldDefn.fieldType) {
         case DbRecords::FieldType::Bool:
            break;
         case DbRecords::FieldType::Int:
            break;
         case DbRecords::FieldType::UInt:
            break;
         case DbRecords::FieldType::Double:
            break;
         case DbRecords::FieldType::String:
            break;
         case DbRecords::FieldType::Date:
            break;
         case DbRecords::FieldType::Enum:
            break;
         default:
            break;
      }

      //
      // .:TODO:. Primary key is "id INTEGER PRIMARY KEY autoincrement"
      //
   }
   queryStringAsStream << "\n);";

   // *****************************************************************************************************************
   // .:TODO:. Junction tables
   // *****************************************************************************************************************
   return;
}

void DbRecords::loadAll(QSqlDatabase databaseConnection) {
   //
   // NB: We need to have QSqlDatabase passed in because we can't call Database::instance().sqlDatabase() because we
   // are ourselves being called from the first call to Database::instance() which invokes Database::load()
   //
   // .:TBD:. One day we should change where the loadAll calls are made from so that the Database class has no
   //         knowledge of DbRecords / DbNamedEntityRecords classes.
   //

   // Start transaction
   // (By the magic of RAII, this will abort if we return from this function without calling dbTransaction.commit()
   DbTransaction dbTransaction{databaseConnection};

   //
   // Using QSqlTableModel would save us having to write a SELECT statement, however it is a bit hard to use it to
   // reliably get the number of rows in a table.  Eg, QSqlTableModel::rowCount() is not implemented for all databases,
   // and there is no documented way to detect the index supplied to QSqlTableModel::record(int row) is valid.  (In
   // testing with SQLite, the returned QSqlRecord object for an index one beyond the end of he table still gave a
   // false return to QSqlRecord::isEmpty() but then returned invalid record values.)
   //
   // So, instead, we create the appropriate SELECT query from scratch.  We specify the column names rather than just
   // do SELECT * because it's small extra effort and will give us an early error if an invalid column is specified.
   //
   QString queryString{"SELECT "};
   QTextStream queryStringAsStream{&queryString};
   this->pimpl->appendColumNames(queryStringAsStream, true, false);
   queryStringAsStream << "\n FROM " << this->pimpl->tableName << ";";
   QSqlQuery sqlQuery{queryString, databaseConnection};
   if (!sqlQuery.exec()) {
      qCritical() <<
         Q_FUNC_INFO << "Error executing database query " << queryString << ": " << sqlQuery.lastError().text();
      return;
   }

   qDebug() << Q_FUNC_INFO << "Reading main table rows from database query " << queryString;

   while (sqlQuery.next()) {
      //
      // We want to pull all the fields for the current row from the database and use them to construct a new
      // object.
      //
      // Two approaches suggest themselves:
      //
      //    (i)  Create a blank object and, using Qt Properties, fill in each field using the QObject setProperty()
      //         call (as we currently do when reading in an XML file).
      //    (ii) Read all the fields for this row from the database and then use them as parameters to call a
      //         suitable constructor to get a new object.
      //
      // The problem with approach (i) is that lots of the setters called via setProperty have side-effects
      // including emitting signals and trying to update the database.  We can sort of get away with ignoring this
      // while reading an XML file, but we risk going round in circles (including being deadlocked) if we let such
      // things happen while we're still reading everything out of the DB at start-up.  A solution would be to have
      // an "initialising" flag on the object that turns off setter side-effects.  This is a small change but one
      // that needs to be made in a lot of places, including almost every setter function.
      //
      // The problem with approach (ii) is that we don't want a constructor that takes a long list of parameters as
      // it's too easy to get bugs where a call is made with the parameters in the wrong order.  We can't easily use
      // Boost Parameter to solve this because it would be hard to have parameter names as pure data (one of the
      // advantages of the Qt Property system), plus it would apparently make compile times very long.  So we would
      // have to roll our own way of passing, say, a QHash (of propertyName -> QVariant) to a constructor.  This is
      // a chunkier change but only needs to be made in a small number of places (new constructors).
      //
      // Although (i) has the further advantage of not requiring a constructor update when a new property is added
      // to a class, it feels a bit wrong to construct an object in "invalid" state and then set a "now valid" flag
      // later after calling lots of setters.  In particular, it is hard (without adding lots of complexity) for the
      // object class to enforce mandatory construction parameters with this approach.
      //
      // Method (ii) is therefore our preferred approach.  We use NamedParameterBundle, which is a simple extension of
      // QHash.
      //
      NamedParameterBundle namedParameterBundle;
      int primaryKey = -1;

      //
      // Populate all the fields
      // By convention, the primary key should be listed as the first field
      //
      // NB: For now we're assuming that the primary key is always an integer, but it would not be enormous work to
      //     allow a wider range of types.
      //
      bool readPrimaryKey = false;
      for (auto const & fieldDefn : this->pimpl->fieldSimpleDefns) {
         //qDebug() << Q_FUNC_INFO << "Reading " << fieldDefn.columnName << " into " << fieldDefn.propertyName;
         QVariant fieldValue = sqlQuery.value(fieldDefn.columnName);
         if (!fieldValue.isValid()) {
            qCritical() <<
               Q_FUNC_INFO << "Error reading column " << fieldDefn.columnName << " (" << fieldValue.toString() <<
               ") from database table " << this->pimpl->tableName << ". SQL error message: " <<
               sqlQuery.lastError().text();
            break;
         }

         // Enums need to be converted from their string representation in the DB to a numeric value
         if (fieldDefn.fieldType == DbRecords::Enum) {
            fieldValue = QVariant(stringToEnum(fieldDefn, fieldValue));
         }

         // It's a coding error if we got the same parameter twice
         Q_ASSERT(!namedParameterBundle.contains(fieldDefn.propertyName));

         namedParameterBundle.insert(fieldDefn.propertyName, fieldValue);

         if (!readPrimaryKey) {
            readPrimaryKey = true;
            primaryKey = fieldValue.toInt();
         }
      }

      // Get a new object...
      auto object = this->createNewObject(namedParameterBundle);

      // ...and store it
      // It's a coding error if we have two objects with the same primary key
      Q_ASSERT(!this->pimpl->allObjects.contains(primaryKey));
      this->pimpl->allObjects.insert(primaryKey, object);
      qDebug() << Q_FUNC_INFO << "Stored " << object->metaObject()->className() << " #" << primaryKey;
   }

   //
   // Now we load the data from the junction tables.  This, pretty much by definition, isn't needed for the object's
   // constructor, so we're OK to pull it out separately.  Otherwise we'd have to do a LEFT JOIN for each junction
   // table in the query above.  Since we're caching everything in memory, and we're not overly worried about
   // optimising every single SQL query (because the amount of data in the DB is not enormous), we prefer the
   // simplicity of separate queries.
   //
   for (auto const & fieldManyToManyDefn : this->pimpl->fieldManyToManyDefns) {
      qDebug() <<
         Q_FUNC_INFO << "Reading junction table " << fieldManyToManyDefn.tableName << " into " <<
         fieldManyToManyDefn.propertyName;

      //
      // Order first by the object we're adding the other IDs to, then order either by the other IDs or by another
      // column if one is specified.
      //
      queryString = "SELECT ";
      queryStringAsStream <<
         fieldManyToManyDefn.thisPrimaryKeyColumn << ", " <<
         fieldManyToManyDefn.otherPrimaryKeyColumn <<
         " FROM " << fieldManyToManyDefn.tableName <<
         " ORDER BY " << fieldManyToManyDefn.thisPrimaryKeyColumn << ", ";
      if (fieldManyToManyDefn.orderByColumn != "") {
         queryStringAsStream << fieldManyToManyDefn.orderByColumn;
      } else {
         queryStringAsStream << fieldManyToManyDefn.otherPrimaryKeyColumn;
      }
      queryStringAsStream << ";";
      sqlQuery = QSqlQuery(queryString, databaseConnection);
      if (!sqlQuery.exec()) {
         qCritical() <<
            Q_FUNC_INFO << "Error executing database query " << queryString << ": " << sqlQuery.lastError().text();
         return;
      }

      qDebug() << Q_FUNC_INFO << "Reading junction table rows from database query " << queryString;

      //
      // The simplest way to process the data is first to build the raw ID-to-ID map in memory...
      //
      QMultiHash<int, QVariant> thisToOtherKeys;
      while (sqlQuery.next()) {
         thisToOtherKeys.insert(sqlQuery.value(fieldManyToManyDefn.thisPrimaryKeyColumn).toInt(),
                                sqlQuery.value(fieldManyToManyDefn.otherPrimaryKeyColumn));
      }

      //
      // ...then loop through the map to pass the data to the relevant objects
      //
      for (int const currentKey : thisToOtherKeys.uniqueKeys()) {
         //
         // It's probably a coding error somewhere if there's an associative entry for an object that doesn't exist,
         // but we can recover by ignoring the associative entry
         //
         if (!this->contains(currentKey)) {
            qCritical() <<
               Q_FUNC_INFO << "Ignoring record in table " << fieldManyToManyDefn.tableName <<
               " for non-existent object with primary key " << currentKey;
            continue;
         }

         auto currentObject = this->getById(currentKey);

         //
         // Normally we'd pass a list of all the "other" keys for each "this" object, but if we've been told to assume
         // there is at most one "other" per "this", then we'll pass just the first one we get back for each "this".
         //
         auto otherKeys = thisToOtherKeys.values(currentKey);
         if (fieldManyToManyDefn.assumeMaxOneEntry) {
            qDebug() <<
               Q_FUNC_INFO << currentObject->metaObject()->className() << " #" << currentKey << ", " <<
               fieldManyToManyDefn.propertyName << "=" << otherKeys.first().toInt();
            currentObject->setProperty(fieldManyToManyDefn.propertyName, otherKeys.first());
         } else {
            //
            // The setProperty function always takes a QVariant, so we need to create one from the QList<QVariant> we
            // have.
            //
            currentObject->setProperty(fieldManyToManyDefn.propertyName, QVariant{otherKeys});
         }
         qDebug() <<
            Q_FUNC_INFO << "Stored " << fieldManyToManyDefn.propertyName << " for " <<
            currentObject->metaObject()->className() << " #" << currentKey;
      }
   }

   dbTransaction.commit();
   return;
}

bool DbRecords::contains(int id) {
   return this->pimpl->allObjects.contains(id);
}

std::shared_ptr<QObject> DbRecords::getById(int id) {
   return this->pimpl->allObjects.value(id);
}

void DbRecords::insert(std::shared_ptr<QObject> object) {
   /// .:TBD:. Do we want parameter to be shared pointer or just a const reference?

   // Start transaction
   // (By the magic of RAII, this will abort if we return from this function without calling dbTransaction.commit()
   QSqlDatabase databaseConnection = Database::instance().sqlDatabase();
   DbTransaction dbTransaction{databaseConnection};

   //
   // Construct the SQL, which will be of the form
   //
   //    INSERT INTO tablename (firstColumn, secondColumn, ...)
   //    VALUES (:firstColumn, :secondColumn, ...);
   //
   // We omit the primary key column because we can't know its value in advance.  We'll find out what value the DB
   // assigned to it after the query was run -- see below.
   //
   // .:TBD:. A small optimisation might be to construct this just once rather than every time this function is called
   //
   QString queryString{"INSERT INTO "};
   QTextStream queryStringAsStream{&queryString};
   queryStringAsStream << this->pimpl->tableName << " (";
   this->pimpl->appendColumNames(queryStringAsStream, false, false);
   queryStringAsStream << ") VALUES (";
   this->pimpl->appendColumNames(queryStringAsStream, false, true);
   queryStringAsStream << ");";

   qDebug() << Q_FUNC_INFO << "Inserting main table row with database query " << queryString;

   //
   // Bind the values
   //
   QSqlQuery sqlQuery{queryString, databaseConnection};
   bool skippedPrimaryKey = false;
   char const * primaryKeyParameter{nullptr};
   for (auto const & fieldDefn: this->pimpl->fieldSimpleDefns) {
      if (!skippedPrimaryKey) {
         // By convention the first field is the primary key
         skippedPrimaryKey = true;
         primaryKeyParameter = fieldDefn.propertyName;
      } else {
         QString bindName = QString{":%1"}.arg(fieldDefn.columnName);
         QVariant bindValue{object->property(fieldDefn.propertyName)};

         // Enums need to be converted to strings first
         if (fieldDefn.fieldType == DbRecords::Enum) {
            bindValue = QVariant{enumToString(fieldDefn, bindValue)};
         }

         sqlQuery.bindValue(bindName, bindValue);
      }
   }

   //
   // The object we are inserting should not already have a valid primary key.
   //
   // .:TBD:. Maybe if we're doing undelete, this is the place to handle that case.
   //
   int currentPrimaryKey = object->property(primaryKeyParameter).toInt();
   Q_ASSERT(currentPrimaryKey <= 0);

   //
   // Run the query
   //
   if (!sqlQuery.exec()) {
      qCritical() <<
         Q_FUNC_INFO << "Error executing database query " << queryString << ": " << sqlQuery.lastError().text();
      return;
   }

   //
   // Get the ID of the row we just inserted and put it in the object
   //
   // Assert that we are only using database drivers that support returning the last insert ID.  (It is frustratingly
   // hard to find documentation about this, as, eg, https://doc.qt.io/qt-5/sql-driver.html does not explicitly list
   // which supplied drivers support which features.  However, in reality, we know SQLite and PostgreSQL drivers both
   // support this, so it would likely only be a problem if a new type of DB were introduced.)
   //
   Q_ASSERT(sqlQuery.driver()->hasFeature(QSqlDriver::LastInsertId));
   auto primaryKey = sqlQuery.lastInsertId();

   object->setProperty(primaryKeyParameter, primaryKey);
   qDebug() << Q_FUNC_INFO << "Object with ID" << primaryKey.toInt() << "inserted in database using" << queryString;

   //
   // Add the object to our list of all objects of this type (asserting that it should be impossible for an object with
   // this ID to already exist in that list).
   //
   Q_ASSERT(!this->pimpl->allObjects.contains(primaryKey.toInt()));
   this->pimpl->allObjects.insert(primaryKey.toInt(), object);

   //
   // Now save data to the junction tables
   //
   for (auto const & fieldManyToManyDefn : this->pimpl->fieldManyToManyDefns) {
      insertIntoFieldManyToManyDefn(fieldManyToManyDefn, *object, primaryKey, databaseConnection);
   }

   //
   // Tell any bits of the UI that need to know that there's a new object
   //
   emit this->signalObjectInserted(primaryKey.toInt());

   dbTransaction.commit();
   return;
}

void DbRecords::update(std::shared_ptr<QObject> object) {
   // Start transaction
   // (By the magic of RAII, this will abort if we return from this function without calling dbTransaction.commit()
   QSqlDatabase databaseConnection = Database::instance().sqlDatabase();
   DbTransaction dbTransaction{databaseConnection};

   //
   // Construct the SQL, which will be of the form
   //
   //    UPDATE tablename
   //    SET firstColumn = :firstColumn, secondColumn = :secondColumn, ...
   //    WHERE primaryKeyColumn = :primaryKeyColumn;
   //
   // .:TBD:. A small optimisation might be to construct this just once rather than every time this function is called
   //
   QString queryString{"UPDATE "};
   QTextStream queryStringAsStream{&queryString};
   queryStringAsStream << this->pimpl->tableName << " SET ";

   // By convention the first field is the primary key
   QString const &    primaryKeyColumn   {this->pimpl->fieldSimpleDefns[0].columnName};
   char const * const primaryKeyProperty {this->pimpl->fieldSimpleDefns[0].propertyName};
   QVariant const     primaryKey         {object->property(primaryKeyProperty)};

   bool skippedPrimaryKey = false;
   bool firstFieldOutput = false;
   for (auto const & fieldDefn: this->pimpl->fieldSimpleDefns) {
      if (!skippedPrimaryKey) {
         skippedPrimaryKey = true;
      } else {
         if (!firstFieldOutput) {
            firstFieldOutput = true;
         } else {
            queryStringAsStream << ", ";
         }
         queryStringAsStream << " " << fieldDefn.columnName << " = :" << fieldDefn.columnName;
      }
   }

   queryStringAsStream << " WHERE " << primaryKeyColumn << " = :" << primaryKeyColumn << ";";

   //
   // Bind the values
   // Note that, because we're using bind names, it doesn't matter that the order in which we do the binds is different
   // than the order in which the fields appear in the query.
   //
   QSqlQuery sqlQuery{queryString, databaseConnection};
   for (auto const & fieldDefn: this->pimpl->fieldSimpleDefns) {
      QString bindName = QString{":%1"}.arg(fieldDefn.columnName);
      QVariant bindValue{object->property(fieldDefn.propertyName)};

      // Enums need to be converted to strings first
      if (fieldDefn.fieldType == DbRecords::Enum) {
         bindValue = QVariant{enumToString(fieldDefn, bindValue)};
      }

      sqlQuery.bindValue(bindName, bindValue);
   }

   //
   // Run the query
   //
   if (!sqlQuery.exec()) {
      qCritical() <<
         Q_FUNC_INFO << "Error executing database query " << queryString << ": " << sqlQuery.lastError().text();
      return;
   }

   //
   // Now update data in the junction tables
   //
   for (auto const & fieldManyToManyDefn : this->pimpl->fieldManyToManyDefns) {
      qDebug() <<
         Q_FUNC_INFO << "Updating property " << fieldManyToManyDefn.propertyName << " in junction table " <<
         fieldManyToManyDefn.tableName;

      //
      // The simplest thing to do with each junction table is to blat any rows relating to the current object and then
      // write out data based on the current property values.  This may often mean we're deleting rows and rewriting
      // them but, for the small quantity of data we're talking about, it doesn't seem worth the complexity of
      // optimising (eg read what's in the DB, compare with what's in the object property, work out what deletes,
      // inserts and updates are needed to sync them, etc.
      //
      if (!deleteFromFieldManyToManyDefn(fieldManyToManyDefn, primaryKey, databaseConnection)) {
         return;
      }
      if (!insertIntoFieldManyToManyDefn(fieldManyToManyDefn, *object, primaryKey, databaseConnection)) {
         return;
      }
   }

   dbTransaction.commit();
   return;
}

void DbRecords::updateProperty(std::shared_ptr<QObject> object, char const * const propertyToUpdateInDb) {
   // Start transaction
   // (By the magic of RAII, this will abort if we return from this function without calling dbTransaction.commit()
   QSqlDatabase databaseConnection = Database::instance().sqlDatabase();
   DbTransaction dbTransaction{databaseConnection};

   // By convention the first field is the primary key
   // We'll need some of this info even if it's a junction table property we're updating
   QString const &    primaryKeyColumn   {this->pimpl->fieldSimpleDefns[0].columnName};
   char const * const primaryKeyProperty {this->pimpl->fieldSimpleDefns[0].propertyName};
   QVariant const primaryKey{object->property(primaryKeyProperty)};

   //
   // First check whether this is a simple property.  (If not we look for it in the ones we store in junction tables.)
   //
   auto matchingFieldDefn = std::find_if(
      this->pimpl->fieldSimpleDefns.begin(),
      this->pimpl->fieldSimpleDefns.end(),
      [propertyToUpdateInDb](FieldSimpleDefn const & fd) {return 0 == std::strcmp(fd.propertyName, propertyToUpdateInDb);}
   );

   if (matchingFieldDefn != this->pimpl->fieldSimpleDefns.end()) {
      //
      // We're updating a simple property
      //
      // Construct the SQL, which will be of the form
      //
      //    UPDATE tablename
      //    SET columnName = :columnName
      //    WHERE primaryKeyColumn = :primaryKeyColumn;
      //
      QString queryString{"UPDATE "};
      QTextStream queryStringAsStream{&queryString};
      queryStringAsStream << this->pimpl->tableName << " SET ";


      QString const & columnToUpdateInDb = matchingFieldDefn->columnName;

      queryStringAsStream << " " << columnToUpdateInDb << " = :" << columnToUpdateInDb;
      queryStringAsStream << " WHERE " << primaryKeyColumn << " = :" << primaryKeyColumn << ";";

      //
      // Bind the values
      //
      QSqlQuery sqlQuery{queryString, databaseConnection};
      QVariant propertyBindValue{object->property(propertyToUpdateInDb)};
      // Enums need to be converted to strings first
      auto fieldDefn = std::find_if(
         this->pimpl->fieldSimpleDefns.begin(),
         this->pimpl->fieldSimpleDefns.end(),
         [propertyToUpdateInDb](FieldSimpleDefn const & fd){return propertyToUpdateInDb == fd.propertyName;}
      );
      // It's a coding error if we're trying to update a property that's not in the field definitions
      Q_ASSERT(fieldDefn != this->pimpl->fieldSimpleDefns.end());
      if (fieldDefn->fieldType == DbRecords::Enum) {
         propertyBindValue = QVariant{enumToString(*fieldDefn, propertyBindValue)};
      }
      sqlQuery.bindValue(QString{":%1"}.arg(columnToUpdateInDb), propertyBindValue);
      sqlQuery.bindValue(QString{":%1"}.arg(primaryKeyColumn), primaryKey);

      //
      // Run the query
      //
      if (!sqlQuery.exec()) {
         qCritical() <<
            Q_FUNC_INFO << "Error executing database query " << queryString << ": " << sqlQuery.lastError().text();
         return;
      }
   } else {
      //
      // The property we've been given isn't a simple property, so look for it in the ones we store in junction tables
      //
      auto matchingFieldManyToManyDefnDefn = std::find_if(
         this->pimpl->fieldManyToManyDefns.begin(),
         this->pimpl->fieldManyToManyDefns.end(),
         [propertyToUpdateInDb](FieldManyToManyDefn const & jt) {return 0 == std::strcmp(jt.propertyName, propertyToUpdateInDb);}
      );

      // It's a coding error if we couldn't find the property either as a simple field or an associative entity
      Q_ASSERT(matchingFieldManyToManyDefnDefn != this->pimpl->fieldManyToManyDefns.end());

      //
      // As elsewhere, the simplest way to update a junction table is to blat any rows relating to the current object and then
      // write out data based on the current property values.
      //
      if (!deleteFromFieldManyToManyDefn(*matchingFieldManyToManyDefnDefn, primaryKey, databaseConnection)) {
         return;
      }
      if (!insertIntoFieldManyToManyDefn(*matchingFieldManyToManyDefnDefn, *object, primaryKey, databaseConnection)) {
         return;
      }
   }

   // If we made it this far then everything worked and we can commit the transaction
   dbTransaction.commit();
   return;
}


void DbRecords::softDelete(int id) {
   this->pimpl->allObjects.remove(id);
   return;
}

//
void DbRecords::hardDelete(int id) {
   QSqlDatabase databaseConnection = Database::instance().sqlDatabase();
   DbTransaction dbTransaction{databaseConnection};

   //
   // Construct the SQL, which will be of the form
   //
   //    DELETE FROM tablename
   //    WHERE primaryKeyColumn = :primaryKeyColumn;
   //
   // .:TBD:. A small optimisation might be to construct this just once rather than every time this function is called
   //
   QString queryString{"DELETE FROM "};
   QTextStream queryStringAsStream{&queryString};
   queryStringAsStream << this->pimpl->tableName;
   QString const & primaryKeyColumn = this->pimpl->fieldSimpleDefns[0].columnName;
   queryStringAsStream << " WHERE " << primaryKeyColumn << " = :" << primaryKeyColumn << ";";

   //
   // Bind the value
   //
   QVariant primaryKey{id};
   QSqlQuery sqlQuery{queryString, databaseConnection};
   QString bindName = QString{":%1"}.arg(primaryKeyColumn);
   sqlQuery.bindValue(bindName, primaryKey);

   //
   // Run the query
   //
   if (!sqlQuery.exec()) {
      qCritical() <<
         Q_FUNC_INFO << "Error executing database query " << queryString << ": " << sqlQuery.lastError().text();
      return;
   }

   //
   // Now remove data in the junction tables
   //
   for (auto const & fieldManyToManyDefn : this->pimpl->fieldManyToManyDefns) {
      if (!deleteFromFieldManyToManyDefn(fieldManyToManyDefn, primaryKey, databaseConnection)) {
         return;
      }
   }

   //
   // Remove the object from the cache
   //
   this->pimpl->allObjects.remove(id);

   dbTransaction.commit();
   return;
}


std::optional< std::shared_ptr<QObject> > DbRecords::findMatching(std::function<bool(std::shared_ptr<QObject>)> const & matchFunction) {
   auto result = std::find_if(this->pimpl->allObjects.begin(), this->pimpl->allObjects.end(), matchFunction);
   if (result == this->pimpl->allObjects.end()) {
      return std::nullopt;
   }
   return *result;
}
