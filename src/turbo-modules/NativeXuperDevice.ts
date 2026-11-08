import type {KeplerTurboModule} from '@amazon-devices/keplerscript-turbomodule-api';
import {TurboModuleRegistry} from '@amazon-devices/keplerscript-turbomodule-api';

export interface XuperDevice extends KeplerTurboModule {
  launch: () => string;
}
export default TurboModuleRegistry.getEnforcing<XuperDevice>('XuperDevice');
