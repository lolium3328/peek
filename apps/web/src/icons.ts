import {
  Activity,
  Battery,
  Check,
  Clock3,
  CloudSun,
  Cpu,
  createIcons,
  Gauge,
  HardDrive,
  MapPin,
  Power,
  Radio,
  RefreshCw,
  RotateCcw,
  Save,
  Settings,
  SlidersHorizontal,
  Smartphone,
  Vibrate,
  Wifi,
  WifiOff,
  Zap
} from "lucide";

const lucideIcons = {
  Activity,
  Battery,
  Check,
  Clock3,
  CloudSun,
  Cpu,
  Gauge,
  HardDrive,
  MapPin,
  Power,
  Radio,
  RefreshCw,
  RotateCcw,
  Save,
  Settings,
  SlidersHorizontal,
  Smartphone,
  Vibrate,
  Wifi,
  WifiOff,
  Zap
};

export function icon(name: string) {
  return `<i data-lucide="${name}" aria-hidden="true"></i>`;
}

export function refreshIcons() {
  createIcons({ icons: lucideIcons });
}
