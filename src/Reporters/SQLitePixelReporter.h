/*
 * SQLiteDbReporter.h
 *
 * Define the SQLiteDbReporter class which is used to insert relevant
 * information from the model into the database during model execution.
 */
#ifndef SQLITEPIXELREPORTER_H
#define SQLITEPIXELREPORTER_H

#include "Core/PropertyMacro.h"
#include "Reporters/SQLiteDbReporter.h"

class SQLitePixelReporter : public SQLiteDbReporter {
  DELETE_COPY_AND_MOVE(SQLitePixelReporter);

protected:
  // Return the character code that indicates the level of genotype records (c:
  // cell, d: district)
  char get_genotype_level() override { return 'C'; }
  void monthly_genome_data(int month_id) override;
  void monthly_infected_individuals(int month_id) override;
  void monthly_site_data(int month_id) override;

public:
  SQLitePixelReporter() = default;
  ~SQLitePixelReporter() override = default;

  // Overrides
  void initialize(int job_number, std::string path) override;
};

#endif
