// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_GL_FUNCTIONS_DATA_VIEW_H_
#define ORBIT_GL_FUNCTIONS_DATA_VIEW_H_

#include <absl/flags/declare.h>
#include <absl/flags/flag.h>

#include <string>
#include <vector>

#include "DataViews/DataView.h"
#include "capture_data.pb.h"

class OrbitApp;

class FunctionsDataView : public orbit_data_views::DataView {
 public:
  explicit FunctionsDataView(OrbitApp* app);

  static const std::string kUnselectedFunctionString;
  static const std::string kSelectedFunctionString;
  static const std::string kFrameTrackString;
  static std::string BuildSelectedColumnsString(OrbitApp* app,
                                                const orbit_client_protos::FunctionInfo& function);

  const std::vector<Column>& GetColumns() override;
  int GetDefaultSortingColumn() override { return kColumnAddressInModule; }
  std::vector<std::string> GetContextMenu(int clicked_index,
                                          const std::vector<int>& selected_indices) override;
  std::string GetValue(int row, int column) override;
  std::string GetLabel() override { return "Functions"; }

  void OnContextMenu(const std::string& action, int menu_index,
                     const std::vector<int>& item_indices) override;
  void AddFunctions(std::vector<const orbit_client_protos::FunctionInfo*> functions);
  void ClearFunctions();

 protected:
  void DoSort() override;
  void DoFilter() override;
  void ParallelFilter();
  [[nodiscard]] const orbit_client_protos::FunctionInfo* GetFunction(int row) const {
    return functions_[indices_[row]];
  }

  std::vector<std::string> m_FilterTokens;

  enum ColumnIndex {
    kColumnSelected,
    kColumnName,
    kColumnSize,
    kColumnModule,
    kColumnAddressInModule,
    kNumColumns
  };

  static const std::string kMenuActionSelect;
  static const std::string kMenuActionUnselect;
  static const std::string kMenuActionEnableFrameTrack;
  static const std::string kMenuActionDisableFrameTrack;
  static const std::string kMenuActionDisassembly;
  static const std::string kMenuActionSourceCode;

 private:
  static bool ShouldShowSelectedFunctionIcon(OrbitApp* app,
                                             const orbit_client_protos::FunctionInfo& function);
  static bool ShouldShowFrameTrackIcon(OrbitApp* app,
                                       const orbit_client_protos::FunctionInfo& function);
  std::vector<const orbit_client_protos::FunctionInfo*> functions_;

  // TODO(b/185090791): This is temporary and will be removed once this data view has been ported
  // and move to orbit_data_views.
  OrbitApp* app_ = nullptr;
};

#endif  // ORBIT_GL_FUNCTIONS_DATA_VIEW_H_
