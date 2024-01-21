
/*
 * SQLiteDistrictReporter.h
 *
 * Override the base DbReporter and log genotype information at the district
 * level.
 */
#ifndef SQLITEDISTRICTREPORTER_H
#define SQLITEDISTRICTREPORTER_H

#include "Helpers/SQLiteDatabase.h"
#include "Reporter.h"

class SQLiteDistrictReporter : public Reporter {
private:
  std::vector<int> district_lookup;
  std::unique_ptr<SQLiteDatabase> db;

private:
  const std::string INSERT_COMMON = R""""(
  INSERT INTO MonthlyData (DaysElapsed, ModelTime, SeasonalFactor)
  VALUES (?, ?, ?)
  RETURNING id;
  )"""";

  const std::string INSERT_SITE_PREFIX = R""""(
    INSERT INTO MonthlySiteData (MonthlyDataId, LocationId, Population, ClinicalEpisodes, Treatments, EIR, PfPrUnder5, PfPr2to10, PfPrAll, TreatmentFailures, NonTreatment, Under5Treatment, Over5Treatment) 
    VALUES 
  )"""";

  const std::string INSERT_GENOTYPE_PREFIX = R"""(
    INSERT INTO MonthlyGenomeData 
    (MonthlyDataId, LocationId, GenomeId, Occurrences, 
    ClinicalOccurrences, Occurrences0to5, Occurrences2to10, 
    WeightedOccurrences) 
    VALUES 
  )""";

  const std::string UPDATE_INFECTED_INDIVIDUALS = R"""(
    UPDATE MonthlySiteData SET InfectedIndividuals = {} 
    WHERE MonthlyDataId = {} AND LocationId = {};
  )""";

  void populate_genotype_table();
  void populate_db_schema();

  void monthly_genome_data(int id);
  void monthly_infected_individuals(int id);
  void monthly_site_data(int id);

protected:
  // Get the genotype level (specific to district level in this case)
  char get_genotype_level() { return 'D'; }

public:
  // Constructor and destructor
  SQLiteDistrictReporter() = default;
  ~SQLiteDistrictReporter() override;

  // Initialize the reporter with job number and path
  void initialize(int job_number, std::string path) override;

  // Basic declarations for before run and begin time step
  void before_run() override {}
  void begin_time_step() override {}

  // Overridden functions for monthly reporting and post-run cleanup
  void monthly_report() override;
  void after_run() override;
};

#endif
