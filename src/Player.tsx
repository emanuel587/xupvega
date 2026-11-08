import * as React from 'react';
import {useEffect, useRef, useState} from 'react';
import {BackHandler, Pressable, StyleSheet, Text, View} from 'react-native';
import {VideoPlayer, KeplerVideoView} from '@amazon-devices/react-native-w3cmedia';

type Props = {title: string; source: string; live: boolean; onBack: () => void};
function clock(n: number): string {
  if (!Number.isFinite(n) || n < 0) return '00:00';
  const secs = Math.floor(n);
  const base = `${String(Math.floor(secs / 60) % 60).padStart(2, '0')}:${String(secs % 60).padStart(2, '0')}`;
  return secs >= 3600 ? `${Math.floor(secs / 3600)}:${base}` : base;
}
export function XuperPlayer({title, source, live, onBack}: Props) {
  const ref = useRef<VideoPlayer | null>(null);
  const [instance, setInstance] = useState<VideoPlayer | null>(null);
  const [status, setStatus] = useState('Preparando video…');
  const [playing, setPlaying] = useState(false);
  const [position, setPosition] = useState(0);
  const [duration, setDuration] = useState(0);
  const [showControls, setShowControls] = useState(true);
  useEffect(() => {
    const back = BackHandler.addEventListener('hardwareBackPress', () => {onBack(); return true;});
    return () => back.remove();
  }, [onBack]);
  useEffect(() => {
    let canceled = false;
    // Vega URL mode no soporta manifiestos adaptativos: requieren MSE + Shaka.
    if (/\.(m3u8|mpd)(?:[?#]|$)/i.test(source)) {
      setStatus('HLS/DASH requiere el módulo Shaka Player de Vega.');
      return;
    }
    const video = new VideoPlayer();
    ref.current = video;
    setInstance(video);
    const onPlay = () => {if (!canceled) {setPlaying(true); setStatus('Reproduciendo');}};
    const onPause = () => {if (!canceled) setPlaying(false);};
    const onTime = () => {if (!canceled) {setPosition(video.currentTime || 0);setDuration(video.duration || 0);}};
    const onError = () => {if (!canceled) setStatus('No se pudo reproducir. Revisa URL, formato y permisos.');};
    video.addEventListener('playing', onPlay);
    video.addEventListener('pause', onPause);
    video.addEventListener('timeupdate', onTime);
    video.addEventListener('loadedmetadata', onTime);
    video.addEventListener('error', onError);
    video.initialize().then(() => {
      if (canceled) return;
      video.src = source;
      return video.play();
    }).catch((error: unknown) => {
      if (!canceled) setStatus('Reproducción: ' + String(error).slice(0, 150));
    });
    return () => {
      canceled = true;
      try {video.pause();} catch (_ignored) {}
      // La decodificación se libera antes de iniciar otro vídeo.
      video.deinitialize?.().catch?.(() => {});
      ref.current = null;
    };
  }, [source]);
  const toggle = async () => {
    const player = ref.current;
    if (!player) return;
    try {if (player.paused) await player.play(); else player.pause();}
    catch (e) {setStatus(String(e));}
    setShowControls(true);
  };
  const seek = (delta: number) => {
    const player = ref.current;
    if (!player || live || !Number.isFinite(player.duration)) return;
    player.currentTime = Math.max(0, Math.min(player.duration, player.currentTime + delta));
    setShowControls(true);
  };
  return <View style={styles.stage}>
    {instance ? <KeplerVideoView style={styles.video} videoPlayer={instance}/> : null}
    <View style={styles.top}>
      <Pressable accessibilityLabel="Volver al catálogo" onPress={onBack} style={styles.topButton}><Text style={styles.topButtonText}>‹  Volver</Text></Pressable>
      <Text numberOfLines={1} style={styles.logo}>XUPER <Text style={styles.logoAccent}>TV NF</Text></Text>
      <Text style={styles.live}>{live ? '● EN VIVO' : 'VIDEO'}</Text>
    </View>
    <View style={styles.bottom}>
      <Text numberOfLines={1} style={styles.heading}>{title}</Text>
      <Text style={styles.subtitle}>{status}</Text>
      {!live && duration > 0 ? <View style={styles.progressTrack}>
        <View style={[styles.progressFill, {width: `${Math.min(100, (position / duration) * 100)}%`}]}/>
      </View> : null}
      <View style={styles.controls}>
        <Pressable onPress={toggle} style={styles.action}><Text style={styles.actionText}>{playing ? 'Ⅱ  Pausar' : '▶  Reproducir'}</Text></Pressable>
        {!live && <>
          <Pressable onPress={() => seek(-10)} style={styles.action}><Text style={styles.actionText}>↶ 10 s</Text></Pressable>
          <Pressable onPress={() => seek(10)} style={styles.action}><Text style={styles.actionText}>10 s ↷</Text></Pressable>
          <Text style={styles.counter}>{clock(position)} / {clock(duration)}</Text>
        </>}
        <View style={{flex:1}}/>
        <Text style={styles.hint}>Pantalla completa · Pulsa Atrás para regresar</Text>
      </View>
    </View>
  </View>;
}
const styles = StyleSheet.create({
  stage:{flex:1,backgroundColor:'#05090e'},
  video:{width:'100%',height:'100%',position:'absolute',top:0,left:0},
  top:{position:'absolute',top:0,left:0,right:0,height:86,backgroundColor:'rgba(5,12,20,.82)',flexDirection:'row',alignItems:'center',paddingHorizontal:28,gap:22},
  topButton:{paddingHorizontal:15,paddingVertical:9,borderRadius:8,backgroundColor:'rgba(255,255,255,.1)'},
  topButtonText:{color:'#fff',fontSize:18},
  logo:{fontSize:24,fontWeight:'900',color:'#fff',letterSpacing:1},
  logoAccent:{color:'#27db7c'},
  live:{color:'#20e082',marginLeft:'auto',fontWeight:'800',fontSize:15},
  bottom:{position:'absolute',left:0,right:0,bottom:0,backgroundColor:'rgba(5,11,20,.87)',paddingHorizontal:40,paddingTop:22,paddingBottom:20},
  heading:{color:'#fff',fontSize:23,fontWeight:'800'},
  subtitle:{color:'#a7b5c5',fontSize:14,marginTop:6},
  progressTrack:{height:5,backgroundColor:'#263345',borderRadius:6,overflow:'hidden',marginTop:16},
  progressFill:{height:5,backgroundColor:'#26d577'},
  controls:{flexDirection:'row',alignItems:'center',gap:12,marginTop:16},
  action:{backgroundColor:'#213041',borderRadius:8,paddingHorizontal:18,paddingVertical:11},
  actionText:{color:'#fff',fontSize:16,fontWeight:'600'},
  counter:{color:'#d0d7df',fontSize:15,marginLeft:7},
  hint:{color:'#afbbcc',fontSize:12},
});
