
/*
 * SQLiteDistrictReporter.h
 *
 * Override the base DbReporter and log genotype information at the district
 * level.
 */
#ifndef SQLITEDISTRICTREPORTER_H
#define SQLITEDISTRICTREPORTER_H

#include "Reporters/SQLiteDbReporter.h"

class SQLiteDistrictReporter : public SQLiteDbReporter {
  DELETE_COPY_AND_MOVE(SQLiteDistrictReporter);

private:
  // Get the genotype level (specific to district level in this case)
  char get_genotype_level() override { return 'D'; }
  void monthly_genome_data(int monthId) override;
  void monthly_site_data(int monthId) override;

public:
  SQLiteDistrictReporter() = default;
  ~SQLiteDistrictReporter() override = default;

  // Initialize the reporter with job number and path
  void initialize(int jobNumber, std::string path) override;
};

#endif
