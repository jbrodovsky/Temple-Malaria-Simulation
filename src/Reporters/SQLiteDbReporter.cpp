#include "SQLiteDbReporter.h"

#include <filesystem>

#include "Core/Config/Config.h"
#include "MDC/MainDataCollector.h"
#include "Model.h"
#include "Population/Population.h"
#include "easylogging++.h"

// Function to populate the 'genotype' table in the database
void SQLiteDbReporter::populate_genotype_table() {
  const std::string INSERT_GENOTYPE =
      "INSERT INTO genotype (id, name) VALUES (?, ?);";

  try {
    // Use the Database class to execute and prepare SQL statements
    db->execute("DELETE FROM genotype;");  // Clear the genotype table

    // Prepare the bulk query
    auto* stmt = db->prepare(INSERT_GENOTYPE);

    auto* config = Model::CONFIG;

    for (auto id = 0; id < config->number_of_parasite_types(); id++) {
      auto* genotype = (*config->genotype_db())[id];
      // Bind values to the prepared statement
      sqlite3_bind_int(stmt, 1, id);
      sqlite3_bind_text(stmt, 2, genotype->to_string(config).c_str(), -1,
                        SQLITE_STATIC);

      if (sqlite3_step(stmt) != SQLITE_DONE) {
        throw std::runtime_error("Error executing INSERT statement");
      }

      sqlite3_reset(stmt);  // Reset the statement for the next iteration
    }

    sqlite3_finalize(stmt);  // Finalize the statement

  } catch (const std::exception &ex) {
    LOG(FATAL) << __FUNCTION__ << "-" << ex.what();
  }
}

// Function to create the database schema
// This sets up the necessary tables in the database
void SQLiteDbReporter::populate_db_schema() {
  // Create the table schema
  const std::string createMonthlyData = R""""(
    CREATE TABLE IF NOT EXISTS monthlydata (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        dayselapsed INTEGER NOT NULL,
        modeltime INTEGER NOT NULL,
        seasonalfactor INTEGER NOT NULL
    );
  )"""";

  std::string age_class_columns;
  for (auto ndx = 0; ndx < Model::CONFIG->age_structure().size(); ndx++) {
    auto ag_from = ndx == 0 ? 0 : Model::CONFIG->age_structure()[ndx - 1];
    auto ag_to = Model::CONFIG->age_structure()[ndx];
    age_class_columns +=
        fmt::format("clinicalepisodes_by_age_class_{}_{}, ", ag_from, ag_to);
  }

  const std::string createMonthlySiteData =
      R""""(
    CREATE TABLE IF NOT EXISTS monthlysitedata (
        monthlydataid INTEGER NOT NULL,
        locationid INTEGER NOT NULL,
        population INTEGER NOT NULL,
        clinicalepisodes INTEGER NOT NULL, )""""
      + age_class_columns +
      R""""(
        treatments INTEGER NOT NULL,
        treatmentfailures INTEGER NOT NULL,
        eir REAL NOT NULL,
        pfprunder5 REAL NOT NULL,
        pfpr2to10 REAL NOT NULL,
        pfprall REAL NOT NULL,
        infectedindividuals INTEGER,
        nontreatment INTEGER NOT NULL,
        under5treatment INTEGER NOT NULL,
        over5treatment INTEGER NOT NULL,
        PRIMARY KEY (monthlydataid, locationid),
        FOREIGN KEY (monthlydataid) REFERENCES monthlydata(id)
    );
  )"""";

  const std::string createGenotype = R""""(
    CREATE TABLE IF NOT EXISTS genotype (
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL
    );
  )"""";

  const std::string createMonthlyGenomeData = R""""(
    CREATE TABLE IF NOT EXISTS monthlygenomedata (
        monthlydataid INTEGER NOT NULL,
        locationid INTEGER NOT NULL,
        genomeid INTEGER NOT NULL,
        occurrences INTEGER NOT NULL,
        clinicaloccurrences INTEGER NOT NULL,
        occurrences0to5 INTEGER NOT NULL,
        occurrences2to10 INTEGER NOT NULL,
        weightedoccurrences REAL NOT NULL,
        PRIMARY KEY (monthlydataid, genomeid, locationid),
        FOREIGN KEY (genomeid) REFERENCES genotype(id),
        FOREIGN KEY (monthlydataid) REFERENCES monthlydata(id)
    );
  )"""";

  try {
    // Use the Database class to execute SQL statements
    db->execute(createMonthlyData);
    db->execute(createMonthlySiteData);
    db->execute(createGenotype);
    db->execute(createMonthlyGenomeData);
  } catch (const std::exception &ex) {
    LOG(ERROR) << "Error in populate_db_schema: " << ex.what();
    // Consider more robust error handling rather than simply logging
  }
}

// Initialize the reporter
// Sets up the database and prepares it for data entry
void SQLiteDbReporter::initialize(int job_number, std::string path) {
  VLOG(1) << "Base SQLiteDbReporter initialized.\n";

  // Define the database file path
  auto dbPath = fmt::format("{}monthly_data_{}.db", path, job_number);

  // Check if the file exists
  if (std::filesystem::exists(dbPath)) {
    // Delete the old database file if it exists
    if (std::remove(dbPath.c_str()) != 0) {
      // Handle the error, if any, when deleting the old file
      LOG(ERROR) << "Error deleting old database file.";
    }
  } else {
    // The file doesn't exist, so no need to delete it
    LOG(INFO) << "Database file does not exist. No deletion needed.\n";
  }

  // Open or create the SQLite database file
  db = std::make_unique<SQLiteDatabase>(dbPath);

  populate_db_schema();
  // populate the genotype table
  populate_genotype_table();

  std::string age_class_columns;
  for (auto ndx = 0; ndx < Model::CONFIG->age_structure().size(); ndx++) {
    auto ag_from = ndx == 0 ? 0 : Model::CONFIG->age_structure()[ndx - 1];
    auto ag_to = Model::CONFIG->age_structure()[ndx];
    age_class_columns +=
        fmt::format("clinicalepisodes_by_age_class_{}_{}, ", ag_from, ag_to);
  }

  // craete insert query based on the age class config
  INSERT_SITE_PREFIX = " INSERT INTO MonthlySiteData (MonthlyDataId, LocationId, "
    "Population, ClinicalEpisodes, " 
    + age_class_columns + 
    " Treatments, EIR, PfPrUnder5, PfPr2to10, PfPrAll, TreatmentFailures,"
    " NonTreatment, Under5Treatment, Over5Treatment) VALUES";
}

void SQLiteDbReporter::monthly_report() {
  // Get the relevant data
  auto days_elapsed = Model::SCHEDULER->current_time();
  auto model_time =
      std::chrono::system_clock::to_time_t(Model::SCHEDULER->calendar_date);
  auto seasonal_factor = Model::CONFIG->seasonal_info()->get_seasonal_factor(
      Model::SCHEDULER->calendar_date, 0);

  auto month_id =
      db->insert_data(INSERT_COMMON, days_elapsed, model_time, seasonal_factor);

  monthly_site_data(month_id);
  if (Model::CONFIG->record_genome_db()
      && Model::MAIN_DATA_COLLECTOR->recording_data()) {
    // Add the genome information, this will also update infected individuals
    monthly_genome_data(month_id);
  } else {
    // If we aren't recording genome data still update the infected
    // individuals
    monthly_infected_individuals(month_id);
  }
}
