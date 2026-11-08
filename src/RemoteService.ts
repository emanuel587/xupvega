/**
 * Cliente del servicio C++ original, integrado en el propio Fire TV Vega OS.
 * No importa M3U y no contiene credenciales privadas del proveedor.
 * Todos los IDs son referencias opacas creadas por el servicio.
 */
const conf = require('../assets/bridge.json') as {host?:string; key?:string};
export type RemoteEntry = {
  id:string; title:string; kind:string; artwork?:string; description?:string;
  program_type?:string; column_id?:number; index?:number;
};
export type RemoteCatalog = {
  title:string; description?:string; artwork?:string; rating?:string;
  release_date?:string; country?:string; genres?:string;
  director?:string; actors?:string; platform?:string; entries:RemoteEntry[];
};
export type RemoteResult = {ok:boolean;status?:string;catalog:RemoteCatalog};
export type RemoteInitial = {ok:boolean;status:string;root:RemoteCatalog;featured:RemoteCatalog};
export type RemotePlay = {ok:boolean;status:string;url?:string;variants?:string[]};
export const configured = /^http:\/\/\d+\.\d+\.\d+\.\d+:\d+$/i.test(conf.host||'') &&
  typeof conf.key==='string' && /^[0-9a-f]{32,128}$/i.test(conf.key);
export class BridgeError extends Error {}
async function request<T>(path:string,ms=30000):Promise<T>{
  if(!configured)throw new BridgeError('El paquete no tiene la configuración privada. Recompila con los JSON de tu instalación Windows.');
  const ctl=new AbortController();
  const id=setTimeout(()=>ctl.abort(),ms);
  try{
    const res=await fetch((conf.host||'')+path,{
      method:'GET',headers:{'X-Xuper-Key':conf.key||''},signal:ctl.signal,
    });
    let json:any;
    try{json=await res.json();}catch(_){throw new BridgeError('El servicio devolvió una respuesta no reconocida.');}
    if(!res.ok||json.ok===false){throw new BridgeError(String(json.status||json.error||`Error HTTP ${res.status}`).slice(0,200));}
    return json as T;
  }catch(err){
    if(err instanceof BridgeError)throw err;
    if((err as Error)?.name==='AbortError')throw new BridgeError('Tiempo de espera agotado. El servicio local de Vega tardó demasiado.');
    throw new BridgeError('No se pudo comunicar con el módulo de contenido del Fire TV.');
  }finally{clearTimeout(id);}
}
export const remote={
  start:()=>request<RemoteInitial>('/api/start',75000),
  open:(id:string)=>request<RemoteResult>('/api/open?id='+encodeURIComponent(id),35000),
  search:(term:string)=>request<RemoteResult>('/api/search?q='+encodeURIComponent(term),150000),
  play:(id:string,variant=0)=>request<RemotePlay>('/api/play?id='+encodeURIComponent(id)+'&variant='+variant,55000),
};
