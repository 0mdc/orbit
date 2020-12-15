// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_GL_APP_H_
#define ORBIT_GL_APP_H_

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <grpc/impl/codegen/connectivity_state.h>
#include <grpcpp/channel.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <outcome.hpp>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "CallStackDataView.h"
#include "CallTreeView.h"
#include "CaptureWindow.h"
#include "DataManager.h"
#include "DataView.h"
#include "DataViewFactory.h"
#include "DataViewTypes.h"
#include "DisassemblyReport.h"
#include "FramePointerValidatorClient.h"
#include "FrameTrackOnlineProcessor.h"
#include "FunctionsDataView.h"
#include "GlCanvas.h"
#include "IntrospectionWindow.h"
#include "MainThreadExecutor.h"
#include "ManualInstrumentationManager.h"
#include "ModulesDataView.h"
#include "OrbitBase/Logging.h"
#include "OrbitBase/Result.h"
#include "OrbitBase/ThreadPool.h"
#include "OrbitCaptureClient/CaptureClient.h"
#include "OrbitCaptureClient/CaptureListener.h"
#include "OrbitClientData/Callstack.h"
#include "OrbitClientData/CallstackTypes.h"
#include "OrbitClientData/ModuleData.h"
#include "OrbitClientData/ModuleManager.h"
#include "OrbitClientData/PostProcessedSamplingData.h"
#include "OrbitClientData/ProcessData.h"
#include "OrbitClientData/TracepointCustom.h"
#include "OrbitClientData/UserDefinedCaptureData.h"
#include "OrbitClientModel/CaptureData.h"
#include "OrbitClientServices/CrashManager.h"
#include "OrbitClientServices/ProcessManager.h"
#include "OrbitClientServices/TracepointServiceClient.h"
#include "PresetLoadState.h"
#include "PresetsDataView.h"
#include "ProcessesDataView.h"
#include "SamplingReport.h"
#include "ScopedStatus.h"
#include "StatusListener.h"
#include "StringManager.h"
#include "SymbolHelper.h"
#include "TextBox.h"
#include "TracepointsDataView.h"
#include "capture_data.pb.h"
#include "preset.pb.h"
#include "services.pb.h"
#include "tracepoint.pb.h"

class OrbitApp final : public DataViewFactory, public CaptureListener {
 public:
  OrbitApp(MainThreadExecutor* main_thread_executor);
  ~OrbitApp() override;

  static std::unique_ptr<OrbitApp> Create(MainThreadExecutor* main_thread_executor);

  void PostInit();
  void MainTick();

  std::string GetCaptureTime();
  std::string GetSaveFile(const std::string& extension);
  void SetClipboard(const std::string& text);
  ErrorMessageOr<void> OnSavePreset(const std::string& file_name);
  ErrorMessageOr<void> OnLoadPreset(const std::string& file_name);
  ErrorMessageOr<void> OnSaveCapture(const std::string& file_name);
  void OnLoadCapture(const std::string& file_name);
  void OnLoadCaptureCancelRequested();

  [[nodiscard]] CaptureClient::State GetCaptureState() const;
  [[nodiscard]] bool IsCapturing() const;

  bool StartCapture();
  void StopCapture();
  void AbortCapture();
  void ClearCapture();
  bool HasCaptureData() const { return capture_data_.has_value(); }
  void SetCaptureData(CaptureData capture_data) { capture_data_ = std::move(capture_data); }
  [[nodiscard]] CaptureData& GetMutableCaptureData() {
    CHECK(capture_data_.has_value());
    return capture_data_.value();
  }
  [[nodiscard]] const CaptureData& GetCaptureData() const {
    CHECK(capture_data_.has_value());
    return capture_data_.value();
  }

  [[nodiscard]] bool HasSampleSelection() const {
    return selection_report_ != nullptr && selection_report_->HasSamples();
  }

  void ToggleDrawHelp();
  void ToggleCapture();
  void LoadFileMapping();
  void ListPresets();
  void RefreshCaptureView();
  void Disassemble(int32_t pid, const orbit_client_protos::FunctionInfo& function);

  void OnCaptureStarted(
      ProcessData&& process,
      absl::flat_hash_map<uint64_t, orbit_client_protos::FunctionInfo> selected_functions,
      TracepointInfoSet selected_tracepoints,
      UserDefinedCaptureData user_defined_capture_data) override;
  void OnCaptureComplete() override;
  void OnCaptureCancelled() override;
  void OnCaptureFailed(ErrorMessage error_message) override;
  void OnTimer(const orbit_client_protos::TimerInfo& timer_info) override;
  void OnKeyAndString(uint64_t key, std::string str) override;
  void OnUniqueCallStack(CallStack callstack) override;
  void OnCallstackEvent(orbit_client_protos::CallstackEvent callstack_event) override;
  void OnThreadName(int32_t thread_id, std::string thread_name) override;
  void OnThreadStateSlice(orbit_client_protos::ThreadStateSliceInfo thread_state_slice) override;
  void OnAddressInfo(orbit_client_protos::LinuxAddressInfo address_info) override;
  void OnUniqueTracepointInfo(uint64_t key,
                              orbit_grpc_protos::TracepointInfo tracepoint_info) override;
  void OnTracepointEvent(orbit_client_protos::TracepointEventInfo tracepoint_event_info) override;

  void OnValidateFramePointers(std::vector<const ModuleData*> modules_to_validate);

  void SetCaptureWindow(CaptureWindow* capture);
  void SetDebugCanvas(GlCanvas* debug_canvas);
  void SetIntrospectionWindow(IntrospectionWindow* canvas);
  void StopIntrospection();

  void SetSamplingReport(
      PostProcessedSamplingData post_processed_sampling_data,
      absl::flat_hash_map<CallstackID, std::shared_ptr<CallStack>> unique_callstacks);
  void SetSelectionReport(
      PostProcessedSamplingData post_processed_sampling_data,
      absl::flat_hash_map<CallstackID, std::shared_ptr<CallStack>> unique_callstacks,
      bool has_summary);
  void SetTopDownView(const CaptureData& capture_data);
  void ClearTopDownView();
  void SetSelectionTopDownView(const PostProcessedSamplingData& selection_post_processed_data,
                               const CaptureData& capture_data);
  void ClearSelectionTopDownView();

  void SetBottomUpView(const CaptureData& capture_data);
  void ClearBottomUpView();
  void SetSelectionBottomUpView(const PostProcessedSamplingData& selection_post_processed_data,
                                const CaptureData& capture_data);
  void ClearSelectionBottomUpView();

  bool SelectProcess(const std::string& process);

  // This needs to be called from the main thread.
  [[nodiscard]] bool IsCaptureConnected(const CaptureData& capture) const;

  [[nodiscard]] bool IsConnectedToInstance() const {
    return grpc_channel_ != nullptr && grpc_channel_->GetState(false) == GRPC_CHANNEL_READY;
  }

  // Callbacks
  using CaptureStartedCallback = std::function<void()>;
  void SetCaptureStartedCallback(CaptureStartedCallback callback) {
    capture_started_callback_ = std::move(callback);
  }
  using CaptureStopRequestedCallback = std::function<void()>;
  void SetCaptureStopRequestedCallback(CaptureStopRequestedCallback callback) {
    capture_stop_requested_callback_ = std::move(callback);
  }
  using CaptureStoppedCallback = std::function<void()>;
  void SetCaptureStoppedCallback(CaptureStoppedCallback callback) {
    capture_stopped_callback_ = std::move(callback);
  }
  using CaptureFailedCallback = std::function<void()>;
  void SetCaptureFailedCallback(CaptureFailedCallback callback) {
    capture_failed_callback_ = std::move(callback);
  }

  using CaptureClearedCallback = std::function<void()>;
  void SetCaptureClearedCallback(CaptureClearedCallback callback) {
    capture_cleared_callback_ = std::move(callback);
  }

  using OpenCaptureCallback = std::function<void()>;
  void SetOpenCaptureCallback(OpenCaptureCallback callback) {
    open_capture_callback_ = std::move(callback);
  }
  using OpenCaptureFailedCallback = std::function<void()>;
  void SetOpenCaptureFailedCallback(OpenCaptureFailedCallback callback) {
    open_capture_failed_callback_ = std::move(callback);
  }
  using OpenCaptureFinishedCallback = std::function<void()>;
  void SetOpenCaptureFinishedCallback(OpenCaptureFinishedCallback callback) {
    open_capture_finished_callback_ = std::move(callback);
  }
  using SelectLiveTabCallback = std::function<void()>;
  void SetSelectLiveTabCallback(SelectLiveTabCallback callback) {
    select_live_tab_callback_ = std::move(callback);
  }
  using DisassemblyCallback = std::function<void(std::string, DisassemblyReport)>;
  void SetDisassemblyCallback(DisassemblyCallback callback) {
    disassembly_callback_ = std::move(callback);
  }
  using ErrorMessageCallback = std::function<void(const std::string&, const std::string&)>;
  void SetErrorMessageCallback(ErrorMessageCallback callback) {
    error_message_callback_ = std::move(callback);
  }
  using WarningMessageCallback = std::function<void(const std::string&, const std::string&)>;
  void SetWarningMessageCallback(WarningMessageCallback callback) {
    warning_message_callback_ = std::move(callback);
  }
  using InfoMessageCallback = std::function<void(const std::string&, const std::string&)>;
  void SetInfoMessageCallback(InfoMessageCallback callback) {
    info_message_callback_ = std::move(callback);
  }
  using TooltipCallback = std::function<void(const std::string&)>;
  void SetTooltipCallback(TooltipCallback callback) { tooltip_callback_ = std::move(callback); }
  using RefreshCallback = std::function<void(DataViewType type)>;
  void SetRefreshCallback(RefreshCallback callback) { refresh_callback_ = std::move(callback); }

  using SamplingReportCallback = std::function<void(DataView*, std::shared_ptr<SamplingReport>)>;
  void SetSamplingReportCallback(SamplingReportCallback callback) {
    sampling_reports_callback_ = std::move(callback);
  }
  void SetSelectionReportCallback(SamplingReportCallback callback) {
    selection_report_callback_ = std::move(callback);
  }

  using CallTreeViewCallback = std::function<void(std::unique_ptr<CallTreeView>)>;
  void SetTopDownViewCallback(CallTreeViewCallback callback) {
    top_down_view_callback_ = std::move(callback);
  }
  void SetSelectionTopDownViewCallback(CallTreeViewCallback callback) {
    selection_top_down_view_callback_ = std::move(callback);
  }
  void SetBottomUpViewCallback(CallTreeViewCallback callback) {
    bottom_up_view_callback_ = std::move(callback);
  }
  void SetSelectionBottomUpViewCallback(CallTreeViewCallback callback) {
    selection_bottom_up_view_callback_ = std::move(callback);
  }
  using TimerSelectedCallback = std::function<void(const orbit_client_protos::TimerInfo*)>;
  void SetTimerSelectedCallback(TimerSelectedCallback callback) {
    timer_selected_callback_ = callback;
  }

  using SaveFileCallback = std::function<std::string(const std::string& extension)>;
  void SetSaveFileCallback(SaveFileCallback callback) { save_file_callback_ = std::move(callback); }
  void FireRefreshCallbacks(DataViewType type = DataViewType::kAll);
  void Refresh(DataViewType type = DataViewType::kAll) { FireRefreshCallbacks(type); }
  using ClipboardCallback = std::function<void(const std::string&)>;
  void SetClipboardCallback(ClipboardCallback callback) {
    clipboard_callback_ = std::move(callback);
  }
  using SecureCopyCallback =
      std::function<ErrorMessageOr<void>(std::string_view, std::string_view)>;
  void SetSecureCopyCallback(SecureCopyCallback callback) {
    secure_copy_callback_ = std::move(callback);
  }
  using ShowEmptyFrameTrackWarningCallback = std::function<void(std::string_view)>;
  void SetShowEmptyFrameTrackWarningCallback(ShowEmptyFrameTrackWarningCallback callback) {
    empty_frame_track_warning_callback_ = std::move(callback);
  }

  void SetStatusListener(StatusListener* listener) { status_listener_ = listener; }

  void SendDisassemblyToUi(std::string disassembly, DisassemblyReport report);
  void SendTooltipToUi(const std::string& tooltip);
  void SendInfoToUi(const std::string& title, const std::string& text);
  void SendWarningToUi(const std::string& title, const std::string& text);
  void SendErrorToUi(const std::string& title, const std::string& text);
  void NeedsRedraw();
  void RenderImGui();

  void LoadModules(
      const std::vector<ModuleData*>& modules,
      absl::flat_hash_map<std::string, std::vector<uint64_t>> function_hashes_to_hook_map = {},
      absl::flat_hash_map<std::string, std::vector<uint64_t>> frame_track_function_hashes_map = {});
  // TODO(170468590): [ui beta] when out of ui beta, clean this up (it should not be necessary to
  // have an argument here, since OrbitApp will always only have one process associated)
  void UpdateProcessAndModuleList(int32_t pid);

  void UpdateAfterSymbolLoading();
  void UpdateAfterCaptureCleared();

  void LoadPreset(const std::shared_ptr<orbit_client_protos::PresetFile>& preset);
  PresetLoadState GetPresetLoadState(
      const std::shared_ptr<orbit_client_protos::PresetFile>& preset) const;
  void FilterTracks(const std::string& filter);

  void CrashOrbitService(orbit_grpc_protos::CrashOrbitServiceRequest_CrashType crash_type);

  void SetGrpcChannel(std::shared_ptr<grpc::Channel> grpc_channel) {
    CHECK(grpc_channel_ == nullptr);
    CHECK(grpc_channel != nullptr);
    grpc_channel_ = std::move(grpc_channel);
  }
  void SetProcessManager(ProcessManager* process_manager) {
    CHECK(process_manager_ == nullptr);
    CHECK(process_manager != nullptr);
    process_manager_ = process_manager;
  }
  void SetTargetProcess(ProcessData* process);
  [[nodiscard]] DataView* GetOrCreateDataView(DataViewType type) override;
  [[nodiscard]] DataView* GetOrCreateSelectionCallstackDataView();

  [[nodiscard]] ProcessManager* GetProcessManager() { return process_manager_; }
  [[nodiscard]] ThreadPool* GetThreadPool() { return thread_pool_.get(); }
  [[nodiscard]] MainThreadExecutor* GetMainThreadExecutor() { return main_thread_executor_; }
  [[nodiscard]] ProcessData* GetMutableTargetProcess() const { return process_; }
  [[nodiscard]] const ProcessData* GetTargetProcess() const { return process_; }
  [[nodiscard]] ManualInstrumentationManager* GetManualInstrumentationManager() {
    return manual_instrumentation_manager_.get();
  }
  [[nodiscard]] ModuleData* GetMutableModuleByPath(const std::string& path) const {
    return module_manager_->GetMutableModuleByPath(path);
  }
  [[nodiscard]] const ModuleData* GetModuleByPath(const std::string& path) const {
    return module_manager_->GetModuleByPath(path);
  }

  // TODO(kuebler): Move them to a separate controler at some point
  void SelectFunction(const orbit_client_protos::FunctionInfo& func);
  void DeselectFunction(const orbit_client_protos::FunctionInfo& func);
  [[nodiscard]] bool IsFunctionSelected(const orbit_client_protos::FunctionInfo& func) const;
  [[nodiscard]] bool IsFunctionSelected(const SampledFunction& func) const;
  [[nodiscard]] bool IsFunctionSelected(uint64_t absolute_address) const;
  [[nodiscard]] const orbit_client_protos::FunctionInfo* GetSelectedFunction(
      uint64_t absolute_address) const;

  void SetVisibleFunctions(absl::flat_hash_set<uint64_t> visible_functions);
  [[nodiscard]] bool IsFunctionVisible(uint64_t function_address);

  [[nodiscard]] uint64_t highlighted_function() const;
  void set_highlighted_function(uint64_t highlighted_function_address);

  [[nodiscard]] ThreadID selected_thread_id() const;
  void set_selected_thread_id(ThreadID thread_id);

  [[nodiscard]] const TextBox* selected_text_box() const;
  void SelectTextBox(const TextBox* text_box);
  void DeselectTextBox();

  [[nodiscard]] uint64_t GetFunctionAddressToHighlight() const;

  void SelectCallstackEvents(
      const std::vector<orbit_client_protos::CallstackEvent>& selected_callstack_events,
      int32_t thread_id);

  void SelectTracepoint(const TracepointInfo& info);
  void DeselectTracepoint(const TracepointInfo& tracepoint);

  [[nodiscard]] bool IsTracepointSelected(const TracepointInfo& info) const;

  // Only enables the frame track in the capture settings (in DataManager) and does not
  // add a frame track to the current capture data.
  void EnableFrameTrack(const orbit_client_protos::FunctionInfo& function);

  // Only disables the frame track in the capture settings (in DataManager) and does
  // not remove the frame track from the capture data.
  void DisableFrameTrack(const orbit_client_protos::FunctionInfo& function);

  [[nodiscard]] bool IsFrameTrackEnabled(const orbit_client_protos::FunctionInfo& function) const;

  // Adds the frame track to the capture settings and also adds a frame track to the current
  // capture data, *if* the captures contains function calls to the function.
  void AddFrameTrack(const orbit_client_protos::FunctionInfo& function);
  // Removes the frame track from the capture settings and also removes the frame track
  // (if it exists) from the capture data.
  void RemoveFrameTrack(const orbit_client_protos::FunctionInfo& function);

  [[nodiscard]] bool HasFrameTrackInCaptureData(
      const orbit_client_protos::FunctionInfo& function) const;

 private:
  ErrorMessageOr<std::filesystem::path> FindSymbolsLocally(const std::filesystem::path& module_path,
                                                           const std::string& build_id);
  void LoadSymbols(const std::filesystem::path& symbols_path, ModuleData* module_data,
                   std::vector<uint64_t> function_hashes_to_hook,
                   std::vector<uint64_t> frame_track_function_hashes);

  void LoadModuleOnRemote(ModuleData* module_data, std::vector<uint64_t> function_hashes_to_hook,
                          std::vector<uint64_t> frame_track_function_hashes,
                          std::string error_message_from_local);
  ErrorMessageOr<void> SelectFunctionsFromHashes(const ModuleData* module,
                                                 const std::vector<uint64_t>& function_hashes);

  ErrorMessageOr<orbit_client_protos::PresetInfo> ReadPresetFromFile(
      const std::filesystem::path& filename);

  ErrorMessageOr<void> SavePreset(const std::string& filename);
  [[nodiscard]] ScopedStatus CreateScopedStatus(const std::string& initial_message);

  ErrorMessageOr<void> GetFunctionInfosFromHashes(
      const ModuleData* module, const std::vector<uint64_t>& function_hashes,
      std::vector<const orbit_client_protos::FunctionInfo*>* function_infos);

  ErrorMessageOr<void> EnableFrameTracksFromHashes(const ModuleData* module,
                                                   const std::vector<uint64_t> function_hashes);
  void AddFrameTrackTimers(const orbit_client_protos::FunctionInfo& function);
  void RefreshFrameTracks();

  std::atomic<bool> capture_loading_cancellation_requested_ = false;

  CaptureStartedCallback capture_started_callback_;
  CaptureStopRequestedCallback capture_stop_requested_callback_;
  CaptureStoppedCallback capture_stopped_callback_;
  CaptureFailedCallback capture_failed_callback_;
  CaptureClearedCallback capture_cleared_callback_;
  OpenCaptureCallback open_capture_callback_;
  OpenCaptureFailedCallback open_capture_failed_callback_;
  OpenCaptureFinishedCallback open_capture_finished_callback_;
  SelectLiveTabCallback select_live_tab_callback_;
  DisassemblyCallback disassembly_callback_;
  ErrorMessageCallback error_message_callback_;
  WarningMessageCallback warning_message_callback_;
  InfoMessageCallback info_message_callback_;
  TooltipCallback tooltip_callback_;
  RefreshCallback refresh_callback_;
  SamplingReportCallback sampling_reports_callback_;
  SamplingReportCallback selection_report_callback_;
  CallTreeViewCallback top_down_view_callback_;
  CallTreeViewCallback selection_top_down_view_callback_;
  CallTreeViewCallback bottom_up_view_callback_;
  CallTreeViewCallback selection_bottom_up_view_callback_;
  SaveFileCallback save_file_callback_;
  ClipboardCallback clipboard_callback_;
  SecureCopyCallback secure_copy_callback_;
  ShowEmptyFrameTrackWarningCallback empty_frame_track_warning_callback_;
  TimerSelectedCallback timer_selected_callback_;

  std::vector<DataView*> panels_;

  std::unique_ptr<ProcessesDataView> processes_data_view_;
  std::unique_ptr<ModulesDataView> modules_data_view_;
  std::unique_ptr<FunctionsDataView> functions_data_view_;
  std::unique_ptr<CallStackDataView> callstack_data_view_;
  std::unique_ptr<CallStackDataView> selection_callstack_data_view_;
  std::unique_ptr<PresetsDataView> presets_data_view_;
  std::unique_ptr<TracepointsDataView> tracepoints_data_view_;

  CaptureWindow* capture_window_ = nullptr;
  IntrospectionWindow* introspection_window_ = nullptr;
  GlCanvas* debug_canvas_ = nullptr;

  std::shared_ptr<SamplingReport> sampling_report_;
  std::shared_ptr<SamplingReport> selection_report_ = nullptr;
  std::map<std::string, std::string> file_mapping_;

  absl::flat_hash_set<std::string> modules_currently_loading_;

  std::shared_ptr<StringManager> string_manager_;
  std::shared_ptr<grpc::Channel> grpc_channel_;

  MainThreadExecutor* main_thread_executor_;
  std::thread::id main_thread_id_;
  std::unique_ptr<ThreadPool> thread_pool_;
  std::unique_ptr<CaptureClient> capture_client_;
  ProcessManager* process_manager_ = nullptr;
  std::unique_ptr<orbit_client_data::ModuleManager> module_manager_;
  std::unique_ptr<DataManager> data_manager_;
  std::unique_ptr<CrashManager> crash_manager_;
  std::unique_ptr<ManualInstrumentationManager> manual_instrumentation_manager_;

  const SymbolHelper symbol_helper_;

  StatusListener* status_listener_ = nullptr;

  ProcessData* process_ = nullptr;

  std::unique_ptr<FramePointerValidatorClient> frame_pointer_validator_client_;

  // TODO(kuebler): This is mostely written during capture by the capture thread on the
  //  CaptureListener parts of App, but may be read also during capturing by all threads.
  //  Currently, it is not properly synchronized (and thus it can't live at DataManager).
  std::optional<CaptureData> capture_data_;

  FrameTrackOnlineProcessor frame_track_online_processor_;
};

#endif  // ORBIT_GL_APP_H_
