\version "2.24.0"
\language english

\include "jianzidef.ly"

% addHarmonic: 用于整体添加泛音标记的指令
#(define (make-script x)
   (make-music 'ArticulationEvent
               'articulation-type x))

#(define (add-script m x)
  (case (ly:music-property m 'name)
     ((NoteEvent) (set! (ly:music-property m 'articulations)
                      (append (ly:music-property m 'articulations)
                         (list (make-script x))))
                   m)
     ((EventChord)(set! (ly:music-property m 'elements)
                      (append (ly:music-property m 'elements)
                         (list (make-script x))))
                   m)
     (else #f)))

#(define (add-harmonic m)
         (add-script m 'flageolet))

addHarmonic = #(define-music-function (music)
                 (ly:music?)
           (map-some-music add-harmonic music))

% put-before: 将参数2置于参数1前的简化指令
#(define-markup-command (put-before layout props arg1 arg2) (markup? markup?)
(interpret-markup layout props
  #{
    \markup \put-adjacent #X #LEFT #arg1 #arg2
  #}))

% 页面与字体设置
\paper {
  top-margin = 20
  bottom-margin = 10
  left-margin = 15
  right-margin = 15
  %system-system-spacing = #'((padding . 6))
  system-system-spacing = #'((basic-distance . 6) (padding . 4) (stretchability . 1e7))

  #(define fonts
    (make-pango-font-tree "Source Han Serif K"
                          "Source Han Sans K"
                          "Sarasa Mono K"
                          (/ staff-height pt 20)))
}

% 全谱标题
\header {
  title = \markup{\fontsize #2 "主标题"}
  subtitle = \markup "副标题"
  poet = "凡一段"
  meter = \markup { \center-column {"仲吕均" "正調定弦"}
    % 定弦的五线谱
    \score{
      \layout {indent = 0}
      \new Staff \with {
        \magnifyStaff #5/7
        \remove Time_signature_engraver
      }
      \relative c, {
        \clef bass
        \cadenzaOn
        % 根据实际定弦修改
        \key f \major
        c1 d f g a c d
        \cadenzaOff
    }}
  }
  composer = "流素鳴桐 打譜"
  arranger = "流素鳴桐 記譜"
}

\markup \vspace #1

global = {
    \textLengthOn	% 减字占横向空间
    \override TextScript.staff-padding = 3.5 % 调整间距以对齐减字
    \key f \major % 根据实际调式修改
}

\score{
\header {
piece="其一"
}
<<
\new Staff \with {
  midiInstrument = "acoustic guitar (nylon)" % 只能说这个音效比较像
  \remove Time_signature_engraver % 去除节拍标记
}
\relative c, {
  \global
  \clef bass
  \tempo 4=50
  \time 4/4
  f4 ( _\"jz:名十勾二"
  g ) _\"jzv:上,九"
  \addHarmonic{
    a _\markup\put-before\"jz:名十勾二"\"jzv:泛起"
    b _\"jz:勾三"
    c _\markup{\"jz:大九挑四"\"jzv:泛止"}
  }
  \grace{
    d32 _\"jz:散历六五"
    e
  }
  f4 _\"jz:勾四"
  g _\"jz:掐起"
  <a c> _\"jz:散撮二七"
}
>>
\midi{}
\layout{}
}