\version "2.24.0"
\header { tagline = ##f }
\score {
  <<
    \new Staff \with { instrumentName = "Soprano" } {
      \clef "treble"
      \tempo 4 = 120
      c''4 b'4 
    }
    \new Staff \with { instrumentName = "Alto" } {
      \clef "treble"
      g'4 g'4 
    }
    \new Staff \with { instrumentName = "Tenor" } {
      \clef "treble_8"
      e'4 d'4 
    }
    \new Staff \with { instrumentName = "Bass" } {
      \clef "bass"
      c4 g,4 
    }
  >>
  \layout { }
}
