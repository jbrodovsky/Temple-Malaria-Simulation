#include "TurnOffMutationEvent.h"

#include "Core/Config/Config.h"
#include "Core/Scheduler.h"
#include "Model.h"
#include "easylogging++.h"

TurnOffMutationEvent::TurnOffMutationEvent(const int &at_time) {
  time = at_time;
}

void TurnOffMutationEvent::execute() {
  for (auto &it : *Model::CONFIG->drug_db()) { it.second->p_mutation() = 0.0; }
  LOG(INFO) << date::year_month_day{scheduler->calendar_date}
            << " : turn mutation off";
}
