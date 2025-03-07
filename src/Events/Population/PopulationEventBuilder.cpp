/**
 * PopulationEventBuilder.cpp
 *
 * Implement the population event builder factory, and most of the functions
 * specific to producing event objects. More complex functions may be found in
 * separate files under the EventBuilders directory.
 */
#include "PopulationEventBuilder.h"

#include <algorithm>
#include <iostream>
#include <vector>

#include "AnnualBetaUpdateEvent.hxx"
#include "AnnualCoverageUpdateEvent.hxx"
#include "ChangeCirculationPercentEvent.hxx"
#include "ChangeStrategyEvent.h"
#include "ChangeTreatmentCoverageEvent.h"
#include "Core/Config/Config.h"
#include "DistrictImportationDailyEvent.h"
#include "ImportationEvent.h"
#include "ImportationPeriodicallyEvent.h"
#include "ImportationPeriodicallyRandomEvent.h"
#include "IntroduceAQMutantEvent.h"
#include "IntroduceLumefantrineMutantEvent.h"
#include "IntroduceMutantEvent.hxx"
#include "IntroduceMutantRasterEvent.hxx"
#include "IntroducePlas2CopyParasiteEvent.h"
#include "Model.h"
#include "ModifyNestedMFTEvent.h"
#include "RotateStrategyEvent.h"
#include "SingleRoundMDAEvent.h"
#include "TurnOffMutationEvent.h"
#include "TurnOnMutationEvent.h"
#include "UpdateBetaRasterEvent.hxx"
#include "yaml-cpp/yaml.h"

// Disable data flow analysis (DFA) in CLion
#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCDFAInspection"

std::vector<Event*> PopulationEventBuilder::build_introduce_parasite_events(
    const YAML::Node &node, Config* config) {
  std::vector<Event*> events;
  for (const auto &entry : node) {
    auto location = entry["location"].as<int>();
    if (static_cast<std::size_t>(location) < config->number_of_locations()) {
      for (std::size_t j = 0; j < entry["parasite_info"].size(); j++) {
        auto genotype_id = entry["parasite_info"][j]["genotype_id"].as<int>();
        auto num = entry["parasite_info"][j]["number_of_cases"].as<int>();

        const auto starting_date =
            entry["parasite_info"][j]["day"].as<date::year_month_day>();
        auto time = (date::sys_days{starting_date}
                     - date::sys_days{config->starting_date()})
                        .count();

        auto* event = new ImportationEvent(location, time, genotype_id, num);
        events.push_back(event);
      }
    }
  }
  return events;
}

std::vector<Event*>
PopulationEventBuilder::build_introduce_parasites_periodically_events(
    const YAML::Node &node, Config* config) {
  std::vector<Event*> events;

  for (const auto &entry : node) {
    const auto location = entry["location"].as<unsigned long>();
    const unsigned long location_from = location;
    const auto location_to =
        std::min(location + 1, config->number_of_locations());

    for (auto loc = location_from; loc < location_to; ++loc) {
      for (std::size_t j = 0; j < entry["parasite_info"].size(); j++) {
        //            InitialParasiteInfo ipi;
        //            ipi.location = location;
        const auto genotype_id = -1;
        // TODO: implement new importation parasite genotype based on allele
        // distribution

        const auto dur = entry["parasite_info"][j]["duration"].as<int>();
        const auto num = entry["parasite_info"][j]["number_of_cases"].as<int>();

        const auto starting_date =
            entry["parasite_info"][j]["start_day"].as<date::year_month_day>();
        auto time = (date::sys_days{starting_date}
                     - date::sys_days{config->starting_date()})
                        .count();

        auto* event = new ImportationPeriodicallyEvent(
            static_cast<int>(loc), dur, genotype_id, num, time);
        events.push_back(event);
      }
    }
  }
  return events;
}

std::vector<Event*>
PopulationEventBuilder::build_change_treatment_coverage_event(
    const YAML::Node &node, Config* config) {
  std::vector<Event*> events;
  for (const auto &entry : node) {
    auto* tcm = ITreatmentCoverageModel::build(entry, config);
    auto* e = new ChangeTreatmentCoverageEvent(tcm);
    events.push_back(e);
  }
  return events;
}

std::vector<Event*>
PopulationEventBuilder::build_change_treatment_strategy_event(
    const YAML::Node &node, Config* config) {
  std::vector<Event*> events;
  for (const auto &entry : node) {
    const auto starting_date = entry["day"].as<date::year_month_day>();
    auto time = (date::sys_days{starting_date}
                 - date::sys_days{config->starting_date()})
                    .count();
    auto strategy_id = entry["strategy_id"].as<int>();

    // Verify that the strategy id is valid, if not fail
    if (strategy_id >= config->strategy_db.value_.size()) {
      LOG(ERROR) << "Invalid strategy_id! " << strategy_id
                 << " supplied, but strategy_db size is "
                 << config->strategy_db.value_.size();
      exit(-1);
    }

    auto* e = new ChangeStrategyEvent(time, strategy_id);
    events.push_back(e);
  }

  return events;
}

std::vector<Event*>
PopulationEventBuilder::build_rotate_treatment_strategy_event(
    const YAML::Node &node, Config* config) {
  try {
    std::vector<Event*> events;
    for (const auto &entry : node) {
      // Load the values
      auto start_date = entry["day"].as<date::year_month_day>();
      auto time =
          (date::sys_days{start_date} - date::sys_days{config->starting_date()})
              .count();
      auto years = entry["years"].as<int>();
      auto first_strategy_id = entry["first_strategy_id"].as<int>();
      auto second_strategy_id = entry["second_strategy_id"].as<int>();

      // Make sure the years are reasonable
      if (years < 1) {
        LOG(ERROR)
            << "Strategy rotation must be at least one year (whole numbers) in "
            << RotateStrategyEvent::EventName;
        throw std::invalid_argument("Strategy rotation years less than one");
      }

      // Check to make sure the strategy ids are valid
      if (first_strategy_id < 0 || second_strategy_id < 0) {
        LOG(ERROR) << "Strategy id cannot be less than zero for "
                   << RotateStrategyEvent::EventName;
        throw std::invalid_argument("Strategy id cannot be less than zero");
      }
      if (first_strategy_id >= Model::CONFIG->strategy_db().size()
          || second_strategy_id >= Model::CONFIG->strategy_db().size()) {
        LOG(ERROR) << "Strategy id should be less than the total number of "
                      "strategies (zero-indexing) for "
                   << RotateStrategyEvent::EventName;
        throw std::invalid_argument(
            "Strategy id greater than strategy_db size");
      }

      // Log and add the event to the queue
      auto* event = new RotateStrategyEvent(time, years, first_strategy_id,
                                            second_strategy_id);
      VLOG(1) << "Adding " << event->name() << " start: " << start_date
              << ", rotation schedule: " << years
              << ", initial strategy: " << first_strategy_id
              << ", next strategy: " << second_strategy_id;
      events.push_back(event);
    }
    return events;
  } catch (YAML::BadConversion &error) {
    LOG(ERROR) << "Unrecoverable error parsing YAML value in "
               << RotateStrategyEvent::EventName << " node: " << error.msg;
    exit(EXIT_FAILURE);
  }
}

std::vector<Event*> PopulationEventBuilder::build_single_round_mda_event(
    const YAML::Node &node, Config* config) {
  std::vector<Event*> events;
  for (const auto &entry : node) {
    const auto starting_date = entry["day"].as<date::year_month_day>();
    auto time = (date::sys_days{starting_date}
                 - date::sys_days{config->starting_date()})
                    .count();
    auto* e = new SingleRoundMDAEvent(time);
    for (std::size_t loc = 0; loc < config->number_of_locations(); loc++) {
      auto input_loc = entry["fraction_population_targeted"].size()
                               < config->number_of_locations()
                           ? 0
                           : loc;
      e->fraction_population_targeted.push_back(
          entry["fraction_population_targeted"][input_loc].as<double>());
    }

    e->days_to_complete_all_treatments =
        entry["days_to_complete_all_treatments"].as<int>();
    events.push_back(e);
  }

  return events;
}

std::vector<Event*>
PopulationEventBuilder::build_modify_nested_mft_strategy_event(
    const YAML::Node &node, Config* config) {
  std::vector<Event*> events;
  for (const auto &entry : node) {
    const auto starting_date = entry["day"].as<date::year_month_day>();
    auto time = (date::sys_days{starting_date}
                 - date::sys_days{config->starting_date()})
                    .count();
    auto strategy_id = entry["strategy_id"].as<int>();

    auto* e = new ModifyNestedMFTEvent(time, strategy_id);
    events.push_back(e);
  }

  return events;
}

std::vector<Event*> PopulationEventBuilder::build_turn_on_mutation_event(
    const YAML::Node &node, Config* config) {
  try {
    std::vector<Event*> events;
    for (const auto &entry : node) {
      const auto starting_date = entry["day"].as<date::year_month_day>();
      auto time = (date::sys_days{starting_date}
                   - date::sys_days{config->starting_date()})
                      .count();
      double mutation_probability =
          entry["mutation_probability"]
              ? entry["mutation_probability"].as<double>()
              : Model::CONFIG->drug_db()->at(0)->p_mutation();

      int drug_id = entry["drug_id"] ? entry["drug_id"].as<int>() : -1;

      auto* e = new TurnOnMutationEvent(time, mutation_probability, drug_id);
      events.push_back(e);
    }

    return events;
  } catch (YAML::BadConversion &error) {
    LOG(ERROR)
        << "Unrecoverable error parsing YAML value in turn_on_mutation node: "
        << error.msg;
    exit(EXIT_FAILURE);
  }
}

std::vector<Event*> PopulationEventBuilder::build_turn_off_mutation_event(
    const YAML::Node &node, Config* config) {
  std::vector<Event*> events;
  for (const auto &entry : node) {
    const auto starting_date = entry["day"].as<date::year_month_day>();
    auto time = (date::sys_days{starting_date}
                 - date::sys_days{config->starting_date()})
                    .count();
    auto* e = new TurnOffMutationEvent(time);
    events.push_back(e);
  }

  return events;
}

std::vector<Event*>
PopulationEventBuilder::build_introduce_plas2_parasite_events(
    const YAML::Node &node, Config* config) {
  std::vector<Event*> events;
  for (const auto &entry : node) {
    int location = entry["location"].as<int>();
    if (static_cast<std::size_t>(location) < config->number_of_locations()) {
      auto fraction = entry["fraction"].as<double>();

      const auto starting_date = entry["day"].as<date::year_month_day>();
      auto time = (date::sys_days{starting_date}
                   - date::sys_days{config->starting_date()})
                      .count();

      auto* event =
          new IntroducePlas2CopyParasiteEvent(location, time, fraction);
      events.push_back(event);
    }
  }
  return events;
}

std::vector<Event*>
PopulationEventBuilder::build_introduce_aq_mutant_parasite_events(
    const YAML::Node &node, Config* config) {
  std::vector<Event*> events;
  for (const auto &entry : node) {
    auto location = entry["location"].as<int>();
    if (static_cast<std::size_t>(location) < config->number_of_locations()) {
      auto fraction = entry["fraction"].as<double>();

      const auto starting_date = entry["day"].as<date::year_month_day>();
      auto time = (date::sys_days{starting_date}
                   - date::sys_days{config->starting_date()})
                      .count();

      auto* event = new IntroduceAQMutantEvent(location, time, fraction);
      events.push_back(event);
    }
  }
  return events;
}

std::vector<Event*>
PopulationEventBuilder::build_introduce_lumefantrine_mutant_parasite_events(
    const YAML::Node &node, Config* config) {
  std::vector<Event*> events;
  for (const auto &entry : node) {
    int location = entry["location"].as<int>();
    if (static_cast<std::size_t>(location) < config->number_of_locations()) {
      auto fraction = entry["fraction"].as<double>();

      const auto starting_date = entry["day"].as<date::year_month_day>();
      auto time = (date::sys_days{starting_date}
                   - date::sys_days{config->starting_date()})
                      .count();

      auto* event =
          new IntroduceLumefantrineMutantEvent(location, time, fraction);
      events.push_back(event);
    }
  }
  return events;
}

// Generate a new annual event that adjusts the beta at each location within the
// model, assumes that the YAML node contains a rate of change and start date.
std::vector<Event*> PopulationEventBuilder::build_annual_beta_update_event(
    const YAML::Node &node, Config* config) {
  try {
    // Check the node size
    verify_single_node(node, AnnualBetaUpdateEvent::EventName);

    // Build the event
    auto start_date = node[0]["day"].as<date::year_month_day>();
    auto rate = node[0]["rate"].as<float>();
    auto time =
        (date::sys_days{start_date} - date::sys_days{config->starting_date()})
            .count();
    auto* event = new AnnualBetaUpdateEvent(rate, time);

    // Log and add the event to the queue, only one for the country
    VLOG(1) << "Adding " << event->name() << " start: " << start_date
            << ", rate: " << rate;
    std::vector<Event*> events;
    events.push_back(event);
    return events;
  } catch (YAML::BadConversion &error) {
    LOG(ERROR) << "Unrecoverable error parsing YAML value in "
               << AnnualBetaUpdateEvent::EventName << " node: " << error.msg;
    exit(EXIT_FAILURE);
  }
}

// Generate a new annual event that adjusts the coverage at each location within
// the model, assumes that the YAML node contains a rate of change and start
// date.
std::vector<Event*> PopulationEventBuilder::build_annual_coverage_update_event(
    const YAML::Node &node, Config* config) {
  try {
    // Check the node size
    verify_single_node(node, AnnualCoverageUpdateEvent::EventName);

    // Build the event
    auto start_date = node[0]["day"].as<date::year_month_day>();
    auto rate = node[0]["rate"].as<float>();
    auto time =
        (date::sys_days{start_date} - date::sys_days{config->starting_date()})
            .count();
    auto* event = new AnnualCoverageUpdateEvent(rate, time);

    // Log and add the event to the queue, only one for the country
    VLOG(1) << "Adding " << event->name() << " start: " << start_date
            << ", rate: " << rate;
    std::vector<Event*> events;
    events.push_back(event);
    return events;
  } catch (YAML::BadConversion &error) {
    LOG(ERROR) << "Unrecoverable error parsing YAML value in "
               << AnnualCoverageUpdateEvent::EventName
               << " node: " << error.msg;
    exit(EXIT_FAILURE);
  }
}

std::vector<Event*>
PopulationEventBuilder::build_change_circulation_percent_event(
    const YAML::Node &node, Config* config) {
  try {
    std::vector<Event*> events;
    for (const auto &entry : node) {
      // Load the values
      auto start_date = entry["day"].as<date::year_month_day>();
      auto time =
          (date::sys_days{start_date} - date::sys_days{config->starting_date()})
              .count();
      auto rate = entry["circulation_percent"].as<float>();

      // Make sure the rate makes sense
      if (rate < 0.0) {
        LOG(ERROR) << "The daily population circulation percentage much be "
                      "greater than zero for "
                   << ChangeCirculationPercentEvent::EventName;
        throw std::invalid_argument(
            "Population circulation percentage must be greater than zero");
      }
      if (rate > 1.0) {
        LOG(ERROR) << "The daily population circulation percentage must be "
                      "less than one (i.e., 100%) for "
                   << ChangeCirculationPercentEvent::EventName;
        throw std::invalid_argument(
            "Population circulation percentage must be less than one");
      }

      // Log and add the event to the queue
      auto* event = new ChangeCirculationPercentEvent(rate, time);
      VLOG(1) << "Adding " << event->name() << " start: " << start_date
              << ", rate: " << rate;
      events.push_back(event);
    }
    return events;
  } catch (YAML::BadConversion &error) {
    LOG(ERROR) << "Unrecoverable error parsing YAML value in "
               << ChangeCirculationPercentEvent::EventName
               << " node: " << error.msg;
    exit(EXIT_FAILURE);
  }
}

// Generate a new importation periodically random event that uses a weighted
// random selection to add a new malaria infection with a specific genotype
// to the model.
std::vector<Event*>
PopulationEventBuilder::build_importation_periodically_random_event(
    const YAML::Node &node, Config* config) {
  try {
    std::vector<Event*> events;
    for (const auto &entry : node) {
      // Load the values
      auto start_date = entry["day"].as<date::year_month_day>();
      auto time =
          (date::sys_days{start_date} - date::sys_days{config->starting_date()})
              .count();
      auto genotype_id = entry["genotype_id"].as<int>();
      auto count = entry["count"].as<int>();
      auto log_parasite_density = entry["log_parasite_density"].as<double>();

      // Check to make sure the date is valid
      if (start_date.day() != date::day{1}) {
        LOG(ERROR) << "Event must start on the first of the month for "
                   << ImportationPeriodicallyRandomEvent::EventName;
        throw std::invalid_argument(
            "Event must start on the first of the month");
      }

      // Double check that the genotype id is valid
      if (genotype_id < 0) {
        LOG(ERROR) << "Invalid genotype id supplied for "
                   << ImportationPeriodicallyRandomEvent::EventName
                   << " genotype id cannot be less than zero";
        throw std::invalid_argument("Genotype id cannot be less than zero");
      }
      if (genotype_id >= Model::CONFIG->genotype_db()->size()) {
        LOG(ERROR) << "Invalid genotype id supplied for "
                   << ImportationPeriodicallyRandomEvent::EventName
                   << " genotype id cannot be greater than "
                   << Model::CONFIG->genotype_db()->size() - 1;
        throw std::invalid_argument(
            "Genotype id cannot be greater than genotype_db size");
      }

      // Make sure the count makes sense
      if (count < 1) {
        LOG(ERROR) << "The count of importations must be greater than zero for "
                   << ImportationPeriodicallyRandomEvent::EventName;
        throw std::invalid_argument("Count must be greater than zero");
      }

      // Make sure the log parasite density makes sense
      if (log_parasite_density == 0) {
        LOG(ERROR) << "Invalid log parasite density supplied for "
                   << ImportationPeriodicallyRandomEvent::EventName
                   << "Log10 of zero is undefined.";
        throw std::invalid_argument("Log10 of zero is undefined.");
      }

      // Log and add the event to the queue
      auto* event = new ImportationPeriodicallyRandomEvent(
          genotype_id, time, count, log_parasite_density);
      VLOG(1) << "Adding " << event->name() << " start: " << start_date
              << ", genotype_id: " << genotype_id
              << ", log_parasite_density: " << log_parasite_density;
      events.push_back(event);
    }
    return events;
  } catch (YAML::BadConversion &error) {
    LOG(ERROR) << "Unrecoverable error parsing YAML value in "
               << ImportationPeriodicallyRandomEvent::EventName
               << " node: " << error.msg;
    exit(EXIT_FAILURE);
  }
}

std::vector<Event*> PopulationEventBuilder::build_update_beta_raster_event(
    const YAML::Node &node, Config* config) {
  try {
    std::vector<Event*> events;
    for (const auto &entry : node) {
      // Load the values
      auto start_date = entry["day"].as<date::year_month_day>();
      auto time =
          (date::sys_days{start_date} - date::sys_days{config->starting_date()})
              .count();
      auto filename = entry["beta_raster"].as<std::string>();

      // Make sure the file actually exists
      std::ifstream file;
      file.open(filename);
      if (!file) {
        LOG(ERROR) << "The file indicated, " << filename
                   << ", cannot be found.";
        throw std::invalid_argument("File for "
                                    + UpdateBetaRasterEvent::EventName
                                    + " does not appear to exist.");
      } else {
        file.close();
      }

      // Log and add the event to the queue
      auto* event = new UpdateBetaRasterEvent(filename, time);
      VLOG(1) << "Adding " << event->name() << " start: " << start_date;
      events.push_back(event);
    }
    return events;
  } catch (YAML::BadConversion &error) {
    LOG(ERROR) << "Unrecoverable error parsing YAML value in "
               << UpdateBetaRasterEvent::EventName << " node: " << error.msg;
    exit(EXIT_FAILURE);
  }
}

std::vector<Event*>
PopulationEventBuilder::build_import_district_mutant_daily_events(
    const YAML::Node &node, Config* config) {
  std::vector<Event*> events;
  for (const auto &entry : node) {
    auto district = entry["district"].as<int>();
    auto locus = entry["locus"].as<int>();
    auto mutant_allele = entry["mutant_allele"].as<int>();
    auto daily_rate = entry["daily_rate"].as<double>();

    auto start_date = entry["start_date"].as<date::year_month_day>();
    auto start_day =
        (date::sys_days{start_date} - date::sys_days{config->starting_date()})
            .count();
    auto* event = new DistrictImportationDailyEvent(
        district, locus, mutant_allele, daily_rate, start_day);
    event->dispatcher = nullptr;
    events.push_back(event);
  }
  return events;
}

std::vector<Event*> PopulationEventBuilder::build(const YAML::Node &node,
                                                  Config* config) {
  std::vector<Event*> events;
  const auto name = node["name"].as<std::string>();

  if (name == "introduce_plas2_parasites") {
    events = build_introduce_plas2_parasite_events(node["info"], config);
  }
  if (name == "introduce_aq_mutant_parasites") {
    events = build_introduce_aq_mutant_parasite_events(node["info"], config);
  }

  if (name == "introduce_lumefantrine_mutant_parasites") {
    events = build_introduce_lumefantrine_mutant_parasite_events(node["info"],
                                                                 config);
  }

  if (name == "introduce_parasites") {
    events = build_introduce_parasite_events(node["info"], config);
  }
  if (name == "introduce_parasites_periodically") {
    events =
        build_introduce_parasites_periodically_events(node["info"], config);
  }
  if (name == "change_treatment_coverage") {
    events = build_change_treatment_coverage_event(node["info"], config);
  }

  if (name == "change_treatment_strategy") {
    events = build_change_treatment_strategy_event(node["info"], config);
  }

  if (name == "single_round_MDA") {
    events = build_single_round_mda_event(node["info"], config);
  }

  if (name == "modify_nested_mft_strategy") {
    events = build_modify_nested_mft_strategy_event(node["info"], config);
  }
  if (name == "turn_on_mutation") {
    events = build_turn_on_mutation_event(node["info"], config);
  }
  if (name == "turn_off_mutation") {
    events = build_turn_off_mutation_event(node["info"], config);
  }

  if (name == AnnualBetaUpdateEvent::EventName) {
    events = build_annual_beta_update_event(node["info"], config);
  }
  if (name == AnnualCoverageUpdateEvent::EventName) {
    events = build_annual_coverage_update_event(node["info"], config);
  }
  if (name == ChangeCirculationPercentEvent::EventName) {
    events = build_change_circulation_percent_event(node["info"], config);
  }
  if (name == ImportationPeriodicallyRandomEvent::EventName) {
    events = build_importation_periodically_random_event(node["info"], config);
  }
  if (name == IntroduceMutantEvent::EventName) {
    events = build_introduce_mutant_event(node["info"], config);
  }
  if (name == IntroduceMutantRasterEvent::EventName) {
    events = build_introduce_mutant_raster_event(node["info"], config);
  }
  if (name == UpdateBetaRasterEvent::EventName) {
    events = build_update_beta_raster_event(node["info"], config);
  }
  if (name == RotateStrategyEvent::EventName) {
    events = build_rotate_treatment_strategy_event(node["info"], config);
  }
  if (name == DistrictImportationDailyEvent::EventName) {
    events = build_import_district_mutant_daily_events(node["info"], config);
  }

  return events;
}

void PopulationEventBuilder::verify_single_node(const YAML::Node &node,
                                                const std::string &name) {
  if (node.size() > 1) {
    LOG(ERROR) << "More than one sub node found for " << name;
    throw std::invalid_argument("Multiple sub nodes for single node event.");
  }
}

// Re-enable DFA in CLion
#pragma clang diagnostic pop
