# SCDTR-Project

**Project grade: 19 out of 20**

System Architecture:

![Architecture](https://raw.githubusercontent.com/miguelfazenda/SCDTR-Project/main/architecture.png "Architecture")

Local controller for each luminaire:

![Local controller](https://raw.githubusercontent.com/miguelfazenda/SCDTR-Project/main/controller.JPG "Architecture")

Luminare and it's diagram:

![Luminaire diagram](https://raw.githubusercontent.com/miguelfazenda/SCDTR-Project/main/luminaria_diagrama.jpg "Luminaire diagram")
![Luminaire](https://raw.githubusercontent.com/miguelfazenda/SCDTR-Project/main/luminaria.jpg "Luminaire")

 é proposta uma solução para um sistema de controlo distribuído em tempo real de um sistema de iluminação que permite manter a luminosidade num certo intervalo de luz desejado, em cada área de um escritório. O sistema pretende-se que seja autónomo, no sentido em que se auto-regula em função de perturbações externas, e cooperativo, no sentido em que todos os intervenientes trabalham individualmente para atingir a sua iluminação ideal mas de forma coletiva para atingir o objetivo de minimização de energia. Estas características levantam alguns desafios relacionados com a comunicação e sincronização entre intervenientes, manter a coerência do sistema e a gestão dos processos distribuídos.

Um dos objetivos principais do sistema é minimizar a energia gasta durante o seu funcionamento. De modo a medir a satisfação deste aspeto foram desenvolvidos métodos de medição da energia e potência gastas. A necessidade de bem-estar e comodidade do utilizador são avaliadas segundo as métricas de erro de visibilidade (desvio da referência desejada) e erro de \textit{flicker} (amplitude da oscilação de luminosidade num dado nó).

A solução apresentada foi desenvolvida num modelo em escala reduzida de um escritório. Contudo, foi desenvolvido esforço no sentido de se puder replicar facilmente a solução numa dimensão real (em termos de área do escritório e número de luminárias).


\section{Introdução}
Numa altura em que tanto se fala e trabalha no sentido de criar uma geração mais verde e ecológica as atenções estão mais do que nunca viradas para a consciência energética. É assim pertinente o surgimento de novos sistemas capazes de gerir a iluminação de espaços, de modo a garantir a segurança e conforto dos utilizadores e a iluminação desejada, ao mesmo tempo que garanta a minimização de custos e gastos energéticos. 

É nesta vertente que este projeto surge. É pretendido que o sistema proposto seja implementado, por exemplo, num escritório. Nesta abordagem foi considerado que o escritório possui várias secretárias, cada uma equipada por uma luminária. Cada luminária representa um atuador do sistema distribuído, sendo composto por um LED e um sensor de luminosidade (LDR). Possui ainda um micro-controlador, responsável pelas tarefas computacionais e um dispositivo de comunicação (MCP-2515). A este conjunto de componentes é dado o nome de nó. Ao longo da utilização do sistema, é possível definir o estado de ocupação de cada nó individualmente. O caso em que um nó é definido como ocupado corresponde à situação em que a a respetiva secretária está ocupada e por isso o nível de iluminação desejada deve ser superior a uma referência pré-definida e confortável ao trabalho a desenvolver. O caso em que um nó é definido como desocupado corresponde à situação em que a respetiva secretária está desocupada, e por isso o nível de iluminação mínima deve ser superior a uma referencia, correspondente a uma luz de presença ou de segurança. Estas iluminações de referencia foram proposta na norma Europeia EN 12464-1. Contudo, estes valores foram redimensionados para o modelo de menor dimensão utilizado no desenvolvimento deste projeto.

Tendo em conta o grande objetivo de minimizar o gasto energético é importante que o sistema implementado seja cooperativo. Deste modo, ganha-se um novo grau de liberdade no sentido em que poderá ser benéfico privilegiar o aumento de luminosidade numa das luminárias em detrimento de outro, caso o custo energético de utilizar a primeira for menor do que o custo de utilizar a segunda (por exemplo quando se utiliza na mesma sala lâmpadas LED e incandescentes). O algoritmo escolhido para este efeito foi um algoritmo do tipo \textit{Consensus}, composto por varias iterações, em que cada em cada iteração cada nó participa na nova estimativa fornecendo uma possível solução, segundo o seu ponto de vista.

Pelos facto já mencionados é dedutível que o algoritmo implementado é adequado a um sistema de controlo distribuído (uma vez que cada nó tem um controlador com capacidade de computação independente), em tempo real (uma vez que as ações  devem ser tomadas e implementadas numa janela temporal restrita), descentralizado (uma vez que não existe nenhum nó que explicitamente tenha uma posição superior na hierarquia) e cooperativo (uma vez que, apesar de cada agente ter o seu objetivo individual, os agentes tem a liberdade para alterar o seu comportamento caso isso ajude outro agente). Este tipo de sistema de controlo apresenta algumas vantagens, nomeadamente uma melhor organização modular que facilita não só o crescimento do projeto em número de agentes como também uma gestão e manutenção de componentes menores. Contudo, surge com isto a necessidade de comunicação e sincronização entre os diversos nós, o que leva à necessidade de definir um protocolo de comunicação. No que diz respeito ao algoritmo descentralizado de \textit{Consensus}, para a determinação das referencias de cada luminária, é de notar que tal implica um acréscimo de comunicações necessárias, e por isso um acrescimento também no tempo necessário para a obtenção da solução, quando comparado com a solução centralizada (em que um nó obtém num número bastante mais reduzido de passos a solução a atribuir a cada agente).

Para efeitos de manutenção, reparação ou despistagem de problemas no sistema foi ainda desenvolvido uma aplicação para computador com capacidade de se ligar a um dos nós, que passa deste modo a ser denominado como \textit{hub}. Este nó é responsável por, em adição às suas tarefas habituais, fazer ainda a comunicação entre a aplicação e todos os agentes do sistema.

Ao longo deste relatório será descrito primeiramente a abordagem que foi feita de forma mais detalhada, e serão ainda analisados em detalhe alguns aspetos e ferramentas utilizadas. Será também feita uma analise dos resultados alcançados e, por fim, será dada uma análise geral da solução obtida.
