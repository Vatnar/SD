import { QuartzComponent, QuartzComponentConstructor, QuartzComponentProps } from "./types"
import style from "./styles/navlinks.scss"

interface Options {
  links: Record<string, string>
}

export default ((opts?: Options) => {
  const NavLinks: QuartzComponent = ({ displayClass }: QuartzComponentProps) => {
    const links = opts?.links ?? []
    return (
      <nav class={`nav-links ${displayClass ?? ""}`}>
        {Object.entries(links).map(([text, link]) => (
          <a href={link}>{text}</a>
        ))}
      </nav>
    )
  }

  NavLinks.css = style
  return NavLinks
}) satisfies QuartzComponentConstructor<Options>