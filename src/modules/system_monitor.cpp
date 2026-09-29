#include "modules/system_monitor.h"

#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <netioapi.h>

using nlohmann::json;

namespace {

ULONGLONG ToU64(const FILETIME& ft) {
  return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

}  // namespace

json SystemMonitor::Live() {
  if (!running_) return nullptr;
  json out;

  // Processeur : différence entre deux mesures de GetSystemTimes.
  FILETIME idle, kernel, user;
  if (GetSystemTimes(&idle, &kernel, &user)) {
    const ULONGLONG i = ToU64(idle), total = ToU64(kernel) + ToU64(user);  // kernel inclut idle
    double cpu = 0;
    if (prev_total_ && total > prev_total_) {
      cpu = 100.0 * (1.0 - double(i - prev_idle_) / double(total - prev_total_));
    }
    prev_idle_ = i;
    prev_total_ = total;
    out["cpu"] = cpu < 0 ? 0 : (cpu > 100 ? 100 : cpu);
  }

  SYSTEM_INFO si;
  GetNativeSystemInfo(&si);
  out["cores"] = si.dwNumberOfProcessors;

  MEMORYSTATUSEX mem{sizeof(mem)};
  if (GlobalMemoryStatusEx(&mem)) {
    out["ram"] = {{"used", mem.ullTotalPhys - mem.ullAvailPhys}, {"total", mem.ullTotalPhys}};
  }

  ULARGE_INTEGER free_bytes, total_bytes;
  wchar_t sysdrive[MAX_PATH] = L"C:\\";
  GetWindowsDirectoryW(sysdrive, MAX_PATH);
  sysdrive[3] = L'\0';  // "C:\"
  if (GetDiskFreeSpaceExW(sysdrive, nullptr, &total_bytes, &free_bytes)) {
    out["disk"] = {{"drive", std::string(1, static_cast<char>(sysdrive[0])) + ":"},
                   {"used", total_bytes.QuadPart - free_bytes.QuadPart},
                   {"total", total_bytes.QuadPart}};
  }

  SYSTEM_POWER_STATUS ps;
  if (GetSystemPowerStatus(&ps) && ps.BatteryFlag != 128 && ps.BatteryFlag != 255 && ps.BatteryLifePercent <= 100) {
    out["battery"] = {{"percent", ps.BatteryLifePercent},
                      {"charging", ps.ACLineStatus == 1},
                      {"secondsLeft", ps.BatteryLifeTime == static_cast<DWORD>(-1) ? -1 : static_cast<long long>(ps.BatteryLifeTime)},
                      {"saver", ps.SystemStatusFlag == 1}};
  } else {
    out["battery"] = nullptr;
  }

  // Réseau : total des octets de toutes les cartes physiques actives.
  PMIB_IF_TABLE2 table = nullptr;
  if (GetIfTable2(&table) == NO_ERROR) {
    ULONGLONG in = 0, outb = 0;
    for (ULONG k = 0; k < table->NumEntries; ++k) {
      const auto& row = table->Table[k];
      if (row.InterfaceAndOperStatusFlags.HardwareInterface && row.OperStatus == IfOperStatusUp) {
        in += row.InOctets;
        outb += row.OutOctets;
      }
    }
    FreeMibTable(table);
    const ULONGLONG now = GetTickCount64();
    if (prev_net_tick_ && now > prev_net_tick_ && in >= prev_net_bytes_in_ && outb >= prev_net_bytes_out_) {
      const double sec = (now - prev_net_tick_) / 1000.0;
      out["net"] = {{"down", (in - prev_net_bytes_in_) / sec}, {"up", (outb - prev_net_bytes_out_) / sec}};
    }
    prev_net_bytes_in_ = in;
    prev_net_bytes_out_ = outb;
    prev_net_tick_ = now;
  }

  out["uptime"] = GetTickCount64() / 1000;
  return out;
}
