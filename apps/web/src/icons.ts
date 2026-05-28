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
  Image,
  Layers,
  LayoutDashboard,
  MapPin,
  Monitor,
  Power,
  Radio,
  RefreshCw,
  RotateCcw,
  Save,
  Settings,
  SlidersHorizontal,
  Smartphone,
  Box,
  Trash2,
  Type,
  Upload,
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
  Image,
  Layers,
  LayoutDashboard,
  MapPin,
  Monitor,
  Power,
  Radio,
  RefreshCw,
  RotateCcw,
  Save,
  Settings,
  SlidersHorizontal,
  Smartphone,
  Box,
  Trash2,
  Type,
  Upload,
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
